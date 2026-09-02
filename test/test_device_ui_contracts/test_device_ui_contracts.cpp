#include <unity.h>

#include <array>
#include <string>
#include <vector>

#include "device_ui_contracts.hpp"
#include "device_ui_session.hpp"
#include "device_ui_shell.hpp"
#include "device_ui_text.hpp"
#include "device_ui_theme.hpp"

namespace {

using device_platform::BottomSlot;
using device_platform::BrandingId;
using device_platform::DeviceUiBuildCatalog;
using device_platform::DeviceUiCommandOutcomeCategory;
using device_platform::LocalDeviceShellState;
using device_platform::LocaleId;
using device_platform::LocalNavigationAction;
using device_platform::PageExitRequirement;
using device_platform::ServiceSessionEvent;
using device_platform::ServiceSessionLease;
using device_platform::ServiceSessionPolicy;
using device_platform::ShellRoute;
using device_platform::TextKey;
using device_platform::TextLookupSource;
using device_platform::TextNamespace;
using device_platform::TextPackCapabilities;
using device_platform::TextPackManifest;
using device_platform::TextTranslation;
using device_platform::ThemeDescriptor;
using device_platform::ThemeId;
using device_platform::ThemeResolutionSource;
using device_platform::ThemeToken;
using device_platform::UiSectionAvailability;
using device_platform::UiSectionDescriptor;
using device_platform::UiSectionOwner;

TextKey key(const char* value) { return {TextNamespace{"platform"}, value}; }

DeviceUiBuildCatalog r1Catalog() {
    return {BrandingId{"manuengineer"},
            {BrandingId{"manuengineer"}},
            {LocaleId{"de"}, LocaleId{"en"}, LocaleId{"es"}},
            {ThemeId{"manuengineer-dark"}},
            LocaleId{"en"},
            ThemeId{"manuengineer-dark"}};
}

ThemeDescriptor completeDefaultTheme() {
    return {ThemeId{"manuengineer-dark"},
            {ThemeToken::Canvas, ThemeToken::Surface, ThemeToken::PrimaryAction,
             ThemeToken::SecondaryAction, ThemeToken::TextPrimary,
             ThemeToken::TextSecondary, ThemeToken::StatusInformation,
             ThemeToken::StatusWarning, ThemeToken::StatusError,
             ThemeToken::Overlay, ThemeToken::OnCanvas, ThemeToken::OnSurface,
             ThemeToken::OnPrimaryAction, ThemeToken::OnSecondaryAction,
             ThemeToken::OnStatusInformation, ThemeToken::OnStatusWarning,
             ThemeToken::OnStatusError, ThemeToken::OnOverlay}};
}

void test_build_catalog_and_clock_contract_remain_renderer_independent() {
    const auto catalog = r1Catalog();
    TEST_ASSERT_TRUE(catalog.valid());
    TEST_ASSERT_TRUE(catalog.contains(LocaleId{"de"}));
    TEST_ASSERT_TRUE(catalog.contains(LocaleId{"es"}));
    TEST_ASSERT_TRUE(catalog.contains(ThemeId{"manuengineer-dark"}));

    const device_platform::ClockViewInput unavailable{
        std::nullopt, device_platform::TimeZoneId{"Europe/Zurich"}};
    TEST_ASSERT_FALSE(unavailable.trustedUtc.has_value());
    TEST_ASSERT_EQUAL_STRING("Europe/Zurich",
                             unavailable.canonicalTimeZoneId.value().c_str());
}

void test_text_resolver_uses_active_then_english_then_visible_key() {
    const std::vector<TextPackManifest> packs{
        {TextNamespace{"platform"},
         LocaleId{"de"},
         {"latin", 48U, true},
         {{key("save"), "Speichern"}}},
        {TextNamespace{"platform"},
         LocaleId{"en"},
         {"latin", 48U, true},
         {{key("save"), "Save"}, {key("cancel"), "Cancel"}}}};

    const auto german =
        device_platform::resolveText(packs, LocaleId{"de"}, key("save"));
    TEST_ASSERT_TRUE(german.source == TextLookupSource::ActiveLocale);
    TEST_ASSERT_EQUAL_STRING("Speichern", german.value.c_str());

    const auto englishFallback =
        device_platform::resolveText(packs, LocaleId{"es"}, key("cancel"));
    TEST_ASSERT_TRUE(englishFallback.source ==
                     TextLookupSource::EnglishFallback);
    TEST_ASSERT_EQUAL_STRING("Cancel", englishFallback.value.c_str());

    const auto visibleKey =
        device_platform::resolveText(packs, LocaleId{"es"}, key("missing"));
    TEST_ASSERT_TRUE(visibleKey.source ==
                     TextLookupSource::VisibleTechnicalKey);
    TEST_ASSERT_EQUAL_STRING("platform.missing", visibleKey.value.c_str());
}

void test_theme_falls_closed_to_complete_included_default() {
    const auto catalog = r1Catalog();
    const auto complete = completeDefaultTheme();
    const ThemeDescriptor incomplete{ThemeId{"other"}, {ThemeToken::Canvas}};
    const std::vector<ThemeDescriptor> themes{complete, incomplete};
    const auto fallback =
        device_platform::resolveTheme(themes, catalog, ThemeId{"other"});
    TEST_ASSERT_TRUE(fallback.source ==
                     ThemeResolutionSource::DefaultThemeFallback);
    TEST_ASSERT_TRUE(fallback.descriptor != nullptr);
    TEST_ASSERT_EQUAL_STRING("manuengineer-dark",
                             fallback.descriptor->id.value().c_str());
}

void test_shell_has_exactly_four_slots_and_home_back_hierarchy() {
    LocalDeviceShellState state;
    state.route.segments = {key("status")};
    TEST_ASSERT_TRUE(device_platform::hasExactlyFourVisibleSlots(state));
    TEST_ASSERT_TRUE(device_platform::homeOrBackAction(state.route) ==
                     LocalNavigationAction::Home);
    TEST_ASSERT_TRUE(device_platform::applyLocalNavigation(
        state.route, LocalNavigationAction::Home));
    TEST_ASSERT_TRUE(state.route.segments.empty());

    state.route.segments = {key("status"), key("details")};
    TEST_ASSERT_TRUE(device_platform::homeOrBackAction(state.route) ==
                     LocalNavigationAction::Back);
    TEST_ASSERT_TRUE(device_platform::applyLocalNavigation(
        state.route, LocalNavigationAction::Back));
    TEST_ASSERT_EQUAL_UINT32(1U, state.route.segments.size());
    state.route.exitRequirement = PageExitRequirement::ConfirmDiscard;
    TEST_ASSERT_FALSE(device_platform::applyLocalNavigation(
        state.route, LocalNavigationAction::Home));
}

void test_platform_sections_precede_isolated_application_sections() {
    const device_platform::StaticUiExtensionCatalog catalog{
        {{UiSectionOwner::Application, key("app-status"),
          UiSectionAvailability::Unavailable, key("not-ready")},
         {UiSectionOwner::Platform,
          key("network"),
          UiSectionAvailability::Available,
          {}}}};
    const auto ordered = catalog.orderedSections();
    TEST_ASSERT_EQUAL_UINT32(2U, ordered.size());
    TEST_ASSERT_TRUE(ordered[0].owner == UiSectionOwner::Platform);
    TEST_ASSERT_TRUE(ordered[1].owner == UiSectionOwner::Application);
    TEST_ASSERT_TRUE(ordered[1].availability ==
                     UiSectionAvailability::Unavailable);
}

void test_touch_and_web_session_policies_remain_separate() {
    constexpr std::uint64_t minute = 60U * 1000U;
    ServiceSessionLease touch{{10U * minute, std::nullopt}, 100U};
    TEST_ASSERT_TRUE(touch.activeAt(9U * minute + 100U));
    touch.observe(ServiceSessionEvent::RelevantUserActivity, 9U * minute);
    TEST_ASSERT_TRUE(touch.activeAt(18U * minute));
    TEST_ASSERT_FALSE(touch.activeAt(19U * minute));

    ServiceSessionLease web{{5U * minute, 15U * minute}, 100U};
    web.observe(ServiceSessionEvent::RelevantUserActivity, 14U * minute);
    TEST_ASSERT_FALSE(web.activeAt(15U * minute + 100U));
    ServiceSessionLease invalidated{{10U * minute, std::nullopt}, 0U};
    invalidated.observe(ServiceSessionEvent::SafetyStateInvalidated, 1U);
    TEST_ASSERT_FALSE(invalidated.activeAt(1U));
}

void test_expired_session_activity_cannot_resurrect_or_move_backwards() {
    constexpr std::uint64_t minute = 60U * 1000U;
    ServiceSessionLease touch{{10U * minute, std::nullopt}, 100U};
    touch.observe(ServiceSessionEvent::RelevantUserActivity,
                  10U * minute + 100U);
    TEST_ASSERT_FALSE(touch.activeAt(10U * minute + 100U));
    touch.observe(ServiceSessionEvent::RelevantUserActivity, 11U * minute);
    TEST_ASSERT_FALSE(touch.activeAt(11U * minute));

    ServiceSessionLease web{{5U * minute, 15U * minute}, 100U};
    web.observe(ServiceSessionEvent::RelevantUserActivity, 14U * minute);
    web.observe(ServiceSessionEvent::RelevantUserActivity, 15U * minute);
    TEST_ASSERT_FALSE(web.activeAt(15U * minute));

    ServiceSessionLease backwards{{10U * minute, std::nullopt}, 1000U};
    backwards.observe(ServiceSessionEvent::RelevantUserActivity, 900U);
    TEST_ASSERT_FALSE(backwards.activeAt(1000U));
    backwards.observe(ServiceSessionEvent::RelevantUserActivity, 2000U);
    TEST_ASSERT_FALSE(backwards.activeAt(2000U));
}

void test_command_outcome_categories_stay_bounded() {
    constexpr std::array<DeviceUiCommandOutcomeCategory, 5U> categories{
        DeviceUiCommandOutcomeCategory::Accepted,
        DeviceUiCommandOutcomeCategory::Rejected,
        DeviceUiCommandOutcomeCategory::ConfirmationRequired,
        DeviceUiCommandOutcomeCategory::Busy,
        DeviceUiCommandOutcomeCategory::Unavailable};
    TEST_ASSERT_EQUAL_UINT32(5U, categories.size());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_build_catalog_and_clock_contract_remain_renderer_independent);
    RUN_TEST(test_text_resolver_uses_active_then_english_then_visible_key);
    RUN_TEST(test_theme_falls_closed_to_complete_included_default);
    RUN_TEST(test_shell_has_exactly_four_slots_and_home_back_hierarchy);
    RUN_TEST(test_platform_sections_precede_isolated_application_sections);
    RUN_TEST(test_touch_and_web_session_policies_remain_separate);
    RUN_TEST(test_expired_session_activity_cannot_resurrect_or_move_backwards);
    RUN_TEST(test_command_outcome_categories_stay_bounded);
    return UNITY_END();
}
