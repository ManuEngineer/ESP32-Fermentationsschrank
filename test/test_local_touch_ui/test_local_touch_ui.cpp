#include <unity.h>

#include "device_ui_idle.hpp"
#include "device_ui_pin.hpp"
#include "fermentation_touch_workspace.hpp"
#include "fermentation_ui_projector.hpp"
#include "simulated_device_shell.hpp"

namespace {

using namespace fermentation;

FermentationUiSnapshot snapshotFor(ProcessState state,
                                   FermentationHomeMode expectedMode) {
    static RunCommandState run;
    run = {};
    run.processState.state = state;
    FermentationUiProjectionInput input;
    input.runState = &run;
    input.application.lifecycleState = ApplicationLifecycleState::Ready;
    const auto snapshot = FermentationUiProjector::project(input);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expectedMode),
                          static_cast<int>(snapshot.home.mode));
    return snapshot;
}

void test_workspace_has_fixed_slots_and_manual_paths_are_separate() {
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    const auto home = workspace.view(snapshot);
    TEST_ASSERT_EQUAL_UINT32(4U, home.bottomSlots.size());
    TEST_ASSERT_TRUE(home.bottomSlots[0].enabled);

    const auto holding = workspace.makeManualHoldingIntent({});
    const auto timed = workspace.makeManualTimedIntent({});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Air),
                          static_cast<int>(holding.plan.sensorMode));
    TEST_ASSERT_EQUAL_UINT32(0U, timed.values.durationMinutes);
    TEST_ASSERT_FALSE(holding.plan.maximumProductWaitMinutes.has_value());
    const auto stop = workspace.makeStopIntent(StopOption::AbortAndTurnOff);
    const auto completion = workspace.makeCompletionIntent(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StopOption::AbortAndTurnOff),
                          static_cast<int>(stop.option));
    TEST_ASSERT_FALSE(completion.startCooling);
}

void test_workspace_navigation_does_not_create_a_command_id() {
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    const auto result = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 1U});
    TEST_ASSERT_TRUE(result.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramList),
                          static_cast<int>(workspace.page()));
    TEST_ASSERT_FALSE(result.action.has_value());
}

void test_active_home_press_opens_choice_without_preparing_a_command() {
    const auto snapshot =
        snapshotFor(ProcessState::Fermenting, FermentationHomeMode::ActiveRun);
    FermentationTouchWorkspace workspace;
    const auto result = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 0U});
    TEST_ASSERT_FALSE(result.action.has_value());
    TEST_ASSERT_FALSE(result.transitionAction.has_value());
}

void test_shell_wake_is_first_touch_and_frame_is_deterministic() {
    device_platform_test_support::SimulatedDeviceShell shell;
    TEST_ASSERT_TRUE(shell.frame().valid());
    const auto first = shell.press(
        {device_platform::DeviceUiTargetKind::BottomSlot, 0U}, true, true);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::WakeOnly),
        static_cast<int>(first.outcome));
    TEST_ASSERT_EQUAL_UINT32(2U, shell.trace().size());
    device_platform::DeviceUiIdleController idle(1000U);
    idle.observeUserActivity(10U);
    idle.tick(1010U);
    TEST_ASSERT_TRUE(idle.isSleeping());
}

void test_waiting_exposes_transition_intent_without_local_revision() {
    const auto snapshot = snapshotFor(ProcessState::WaitingForProduct,
                                      FermentationHomeMode::Waiting);
    FermentationTouchWorkspace workspace;
    const auto view = workspace.view(snapshot);
    TEST_ASSERT_TRUE(view.transitionAction.has_value());
    TEST_ASSERT_FALSE(view.action.has_value());
    const auto pressed = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 0U});
    TEST_ASSERT_TRUE(pressed.transitionAction.has_value());
}

void test_pin_model_is_masked_and_owner_states_are_display_only() {
    device_platform::PinEntryModel pin;
    TEST_ASSERT_TRUE(
        pin.apply({device_platform::PinEntryActionKind::Digit, 1U}));
    TEST_ASSERT_TRUE(
        pin.apply({device_platform::PinEntryActionKind::Digit, 2U}));
    TEST_ASSERT_TRUE(
        pin.apply({device_platform::PinEntryActionKind::Digit, 3U}));
    TEST_ASSERT_TRUE(
        pin.apply({device_platform::PinEntryActionKind::Digit, 4U}));
    TEST_ASSERT_EQUAL_STRING("****", pin.masked().c_str());
    TEST_ASSERT_TRUE(
        pin.apply({device_platform::PinEntryActionKind::Commit, 0U}));
    pin.setOwnerState(
        {device_platform::PinEntryState::RetryWait, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::PinEntryState::RetryWait),
        static_cast<int>(pin.state()));
    TEST_ASSERT_EQUAL_STRING("****", pin.masked().c_str());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiSafeBootOwner::Issue57),
        static_cast<int>(safeBootOwnerFor(
            FermentationUiSafeBootTarget::PersistentFactoryReset)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiSafeBootOwner::Issue31),
        static_cast<int>(
            safeBootOwnerFor(FermentationUiSafeBootTarget::RawTouchRecovery)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiSafeBootOwner::Issue89),
        static_cast<int>(safeBootOwnerFor(
            FermentationUiSafeBootTarget::NetworkProvisioningRecovery)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiSafeBootOwner::Issue28),
        static_cast<int>(
            safeBootOwnerFor(FermentationUiSafeBootTarget::DiagnosticsExport)));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_workspace_has_fixed_slots_and_manual_paths_are_separate);
    RUN_TEST(test_workspace_navigation_does_not_create_a_command_id);
    RUN_TEST(test_active_home_press_opens_choice_without_preparing_a_command);
    RUN_TEST(test_shell_wake_is_first_touch_and_frame_is_deterministic);
    RUN_TEST(test_waiting_exposes_transition_intent_without_local_revision);
    RUN_TEST(test_pin_model_is_masked_and_owner_states_are_display_only);
    return UNITY_END();
}
