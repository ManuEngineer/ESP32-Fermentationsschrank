#include <unity.h>

#include <optional>

#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_text.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"

#include "../../main/fermentation_ui_press_dispatcher.hpp"

namespace {

using namespace fermentation;
using namespace fermentation::main_ui;

// Matches the existing test_renderer_boundary.cpp/test_local_touch_ui.cpp
// bottom-slot coordinate convention (index*80 + 20, y=220).
std::uint16_t bottomX(std::uint8_t index) {
    return static_cast<std::uint16_t>(index * 80U + 20U);
}
constexpr std::uint16_t kBottomY = 220U;

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

ManualTimedRunValues validManualTimedValues() {
    ManualTimedRunValues values;
    values.targetTemperatureCelsius = 30.0;
    values.durationMinutes = 60U;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;
    return values;
}

void test_dispatch_no_typed_payload_is_reported_as_such() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    const auto result =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(result.outcome));
    TEST_ASSERT_FALSE(result.prepareStatus.has_value());
    TEST_ASSERT_FALSE(result.confirmStatus.has_value());
    TEST_ASSERT_FALSE(result.resumeFallbackStatus.has_value());
}

void test_dispatch_action_reaches_prepare_and_confirm() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    press.action = FermentationUiEnvelopePayload{
        FermentationUiStartManualTimedIntent{validManualTimedValues()}};
    FermentationUiSnapshot snapshot;
    snapshot.revisions.expectedStateSequence = 0U;

    const auto result =
        dispatchWorkspacePress(fixture.application, snapshot, press, 1000U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WorkspacePressDispatchOutcome::Dispatched),
                          static_cast<int>(result.outcome));
    TEST_ASSERT_TRUE(result.prepareStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(*result.prepareStatus));
    // confirmPrepared() is genuinely, correctly reached (proving both
    // prepare AND confirm are wired, not just prepare): revalidatePrepared
    // Request() re-checks safety/sensor evidence, and this minimal fixture
    // (a bare begin(), no update() tick, no published sensor evidence) has
    // none yet, so the existing, unmodified application logic itself
    // rejects with Unavailable - this dispatcher invents nothing and
    // forwards that real outcome untouched.
    TEST_ASSERT_TRUE(result.confirmStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Unavailable),
        static_cast<int>(*result.confirmStatus));
}

void test_dispatch_resume_fallback_is_forwarded_unmodified() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    // The workspace always produces confirmed=false for a fresh
    // resume-fallback press (see FermentationTouchWorkspace); the
    // dispatcher must forward it exactly, not invent confirmation.
    press.resumeFallback = FermentationUiResumeFallbackCommand{{}, false};

    const auto result =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WorkspacePressDispatchOutcome::Dispatched),
                          static_cast<int>(result.outcome));
    TEST_ASSERT_FALSE(result.prepareStatus.has_value());
    TEST_ASSERT_TRUE(result.resumeFallbackStatus.has_value());
    // No recovery is pending in this fresh application: the existing
    // resumeFallback() path itself rejects with NotInitialized
    // (pendingFallbackResume_ == nullptr), not a status this dispatcher
    // invents.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotInitialized),
        static_cast<int>(*result.resumeFallbackStatus));
}

void test_dispatch_transition_action_is_unavailable_no_owner() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    press.transitionAction = FermentationUiProductInsertedConfirmedIntent{};

    const auto result =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::UnavailableNoOwner),
        static_cast<int>(result.outcome));
}

void test_dispatch_program_edit_is_unavailable_no_owner() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    press.programEdit = FermentationUiProgramEditRequest{
        FermentationUiProgramEditOperation::Reset, "p1", std::nullopt,
        std::nullopt, true};

    const auto result =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::UnavailableNoOwner),
        static_cast<int>(result.outcome));
}

void test_process_touch_without_contact_yields_no_target() {
    AppFixture fixture;
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    const auto packs = makeFermentationUiTextPacks();

    const auto tick = processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/false, 0U, 0U, /*freshPressEdge=*/false, 1000U);

    TEST_ASSERT_FALSE(tick.pressedTarget.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(tick.dispatch.outcome));
}

void test_process_touch_held_without_fresh_edge_does_not_navigate() {
    AppFixture fixture;
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    const auto packs = makeFermentationUiTextPacks();

    const auto tick = processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, bottomX(1), kBottomY,
        /*freshPressEdge=*/false, 1000U);

    TEST_ASSERT_TRUE(tick.pressedTarget.has_value());
    TEST_ASSERT_EQUAL_UINT8(1U, tick.pressedTarget->slotIndex);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(tick.dispatch.outcome));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Home),
                          static_cast<int>(workspace.page()));
}

void test_process_touch_fresh_edge_on_valid_slot_navigates() {
    AppFixture fixture;
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    const auto packs = makeFermentationUiTextPacks();

    // Home/Standby slot 1 is "programs" -> NavigateProgramList (pure
    // navigation, no typed payload).
    const auto tick = processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, bottomX(1), kBottomY,
        /*freshPressEdge=*/true, 1000U);

    TEST_ASSERT_TRUE(tick.pressedTarget.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(tick.dispatch.outcome));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramList),
                          static_cast<int>(workspace.page()));
}

void test_process_touch_fresh_edge_off_target_does_not_navigate() {
    AppFixture fixture;
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    const auto packs = makeFermentationUiTextPacks();

    const auto tick = processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, 20U, 20U, /*freshPressEdge=*/true, 1000U);

    TEST_ASSERT_FALSE(tick.pressedTarget.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Home),
                          static_cast<int>(workspace.page()));
}

}  // namespace

// The native test target does not compile the ESP-IDF main component, so
// the two app-owned main/ sources this test exercises are included here
// directly (the same way test_renderer_boundary.cpp already does for
// fermentation_ui_renderer.cpp). Unlike that narrower test, this one also
// includes fermentation_application.hpp, so PlatformIO's library dependency
// finder already links the full fermentation_app library (including
// fermentation_touch_workspace.cpp) - those production sources are
// therefore deliberately NOT inline-included again here, to avoid duplicate
// symbol definitions.
#include "../../main/fermentation_ui_press_dispatcher.cpp"
#include "../../main/fermentation_ui_renderer.cpp"

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_dispatch_no_typed_payload_is_reported_as_such);
    RUN_TEST(test_dispatch_action_reaches_prepare_and_confirm);
    RUN_TEST(test_dispatch_resume_fallback_is_forwarded_unmodified);
    RUN_TEST(test_dispatch_transition_action_is_unavailable_no_owner);
    RUN_TEST(test_dispatch_program_edit_is_unavailable_no_owner);
    RUN_TEST(test_process_touch_without_contact_yields_no_target);
    RUN_TEST(test_process_touch_held_without_fresh_edge_does_not_navigate);
    RUN_TEST(test_process_touch_fresh_edge_on_valid_slot_navigates);
    RUN_TEST(test_process_touch_fresh_edge_off_target_does_not_navigate);
    return UNITY_END();
}
