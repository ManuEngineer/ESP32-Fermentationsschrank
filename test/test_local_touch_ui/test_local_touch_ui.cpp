#include <unity.h>

#include <algorithm>
#include <array>
#include <utility>

#include "device_platform.hpp"
#include "device_ui_idle.hpp"
#include "device_ui_pin.hpp"
#include "device_ui_session.hpp"
#include "fermentation_application.hpp"
#include "fermentation_touch_workspace.hpp"
#include "fermentation_ui_editing.hpp"
#include "fermentation_ui_projector.hpp"
#include "fermentation_ui_text.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_device_shell.hpp"
#include "simulated_persistent_state_store.hpp"

namespace {

using namespace fermentation;

ProgramCatalog runnableCatalog() {
    auto catalog = makeFactoryProgramCatalog();
    auto& program = catalog.programs.back().program;
    program.fermentationStages.front().targetTemperatureCelsius = 25.0;
    program.fermentationStages.front().durationMinutes = 60U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    program.productSensorFailure.fallbackDelaySeconds = 60U;
    return catalog;
}

FermentationUiSnapshot snapshotWithMessage(ProcessState state, MessageCode code,
                                           bool decisionRequired) {
    RunCommandState run;
    run.processState.state = state;
    run.messages[0].id = 7U;
    run.messages[0].code = code;
    run.messages[0].messageClass = MessageClass::DecisionRequired;
    run.messages[0].decisionRequired = decisionRequired;
    run.messages[0].active = true;
    run.messageCount = 1U;
    FermentationUiProjectionInput input;
    input.runState = &run;
    input.application.lifecycleState = ApplicationLifecycleState::Ready;
    input.primaryAction = fermentationTextKey("acknowledge");
    input.service.available = true;
    return FermentationUiProjector::project(input);
}

device_platform::DeviceUiTarget bottom(std::uint8_t index) {
    return {device_platform::DeviceUiTargetKind::BottomSlot, index};
}

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
    TEST_ASSERT_EQUAL_UINT32(300U, shell.frame().splash.width);
    TEST_ASSERT_EQUAL_UINT32(122U, shell.frame().splash.height);
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
        static_cast<int>(
            FermentationUiSafeBootCapability::PersistentFactoryReset),
        static_cast<int>(safeBootCapabilityFor(
            FermentationUiSafeBootTarget::PersistentFactoryReset)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiSafeBootCapability::RawTouchRecovery),
        static_cast<int>(safeBootCapabilityFor(
            FermentationUiSafeBootTarget::RawTouchRecovery)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            FermentationUiSafeBootCapability::NetworkProvisioningRecovery),
        static_cast<int>(safeBootCapabilityFor(
            FermentationUiSafeBootTarget::NetworkProvisioningRecovery)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiSafeBootCapability::DiagnosticsExport),
        static_cast<int>(safeBootCapabilityFor(
            FermentationUiSafeBootTarget::DiagnosticsExport)));
}

// SIM-26-01, SIM-26-04, SIM-26-05, SIM-26-06, SIM-26-07, SIM-26-09,
// SIM-26-10, SIM-26-11, SIM-26-12, SIM-26-14, SIM-26-21 and SIM-26-47:
// every enabled workspace slot has one route or one existing intent.
void test_sim_26_workspace_action_matrix_and_owner_paths() {
    auto catalog = runnableCatalog();
    auto standby =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    auto home = workspace.view(standby, &catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            FermentationUiWorkspaceSlotAction::NavigateProgramList),
        static_cast<int>(home.slotActions[0]));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateStatus),
        static_cast<int>(home.slotActions[2]));
    TEST_ASSERT_FALSE(home.bottomSlots[3].enabled);
    TEST_ASSERT_TRUE(workspace.press(standby, bottom(0), &catalog).navigated);

    auto active =
        snapshotFor(ProcessState::Fermenting, FermentationHomeMode::ActiveRun);
    workspace.setPage(FermentationUiPage::Home);
    const auto activeHome = workspace.view(active, &catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateStopDialog),
        static_cast<int>(activeHome.slotActions[0]));
    const auto stopPage = workspace.press(active, bottom(0), &catalog);
    TEST_ASSERT_TRUE(stopPage.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::StopDialog),
                          static_cast<int>(workspace.page()));
    const auto stop = workspace.press(active, bottom(1), &catalog);
    TEST_ASSERT_TRUE(stop.action.has_value());
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiStopRunIntent>(*stop.action));

    const auto waiting = snapshotFor(ProcessState::WaitingForProduct,
                                     FermentationHomeMode::Waiting);
    workspace.setPage(FermentationUiPage::Home);
    const auto waitingHome = workspace.view(waiting, &catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            FermentationUiWorkspaceSlotAction::ProductInsertedConfirmed),
        static_cast<int>(waitingHome.slotActions[0]));
    TEST_ASSERT_TRUE(workspace.press(waiting, bottom(0), &catalog)
                         .transitionAction.has_value());

    const auto decision = snapshotWithMessage(
        ProcessState::Fermenting, MessageCode::UserDecisionRequired, true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::Waiting),
                          static_cast<int>(decision.home.mode));
    workspace.setPage(FermentationUiPage::Home);
    const auto decisionHome = workspace.view(decision, &catalog);
    TEST_ASSERT_TRUE(
        decisionHome.slotActions[0] !=
        FermentationUiWorkspaceSlotAction::ProductInsertedConfirmed);
    const auto decisionPress = workspace.press(decision, bottom(0), &catalog);
    TEST_ASSERT_TRUE(decisionPress.navigated);
    TEST_ASSERT_FALSE(decisionPress.transitionAction.has_value());
    const auto decisionDetail = workspace.view(decision, &catalog);
    TEST_ASSERT_TRUE(decisionDetail.bottomSlots[1].enabled);

    auto completed =
        snapshotFor(ProcessState::Completed, FermentationHomeMode::Completed);
    workspace.setPage(FermentationUiPage::Home);
    const auto completedPress = workspace.press(completed, bottom(0), &catalog);
    TEST_ASSERT_TRUE(completedPress.action.has_value());
    TEST_ASSERT_TRUE(std::holds_alternative<FermentationUiCompleteRunIntent>(
        *completedPress.action));
    workspace.setPage(FermentationUiPage::Completion);
    FermentationUiManualRunPlanValues cooling;
    cooling.targetTemperatureCelsius = 8.0;
    cooling.qualificationBandCelsius = 0.5;
    cooling.qualificationDurationMinutes = 1U;
    cooling.maximumTargetReachMinutes = 1U;
    workspace.setCompletionCoolingPlan(cooling);
    const auto completionView = workspace.view(completed, &catalog);
    TEST_ASSERT_TRUE(completionView.completionLocked);
    const auto completionDetails =
        workspace.press(completed, bottom(1), &catalog);
    TEST_ASSERT_TRUE(completionDetails.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Technical),
                          static_cast<int>(workspace.page()));
    workspace.setPage(FermentationUiPage::Completion);
    const auto cool = workspace.press(completed, bottom(3), &catalog);
    TEST_ASSERT_TRUE(cool.action.has_value());
    TEST_ASSERT_TRUE(
        std::get<FermentationUiCompleteRunIntent>(*cool.action).startCooling);
    const auto completionOk = workspace.press(
        completed, {device_platform::DeviceUiTargetKind::Confirm, 0U},
        &catalog);
    TEST_ASSERT_TRUE(completionOk.action.has_value());
    TEST_ASSERT_FALSE(
        std::get<FermentationUiCompleteRunIntent>(*completionOk.action)
            .startCooling);

    workspace.setPage(FermentationUiPage::Recovery);
    auto fallback = snapshotWithMessage(ProcessState::Fermenting,
                                        MessageCode::RecoveryPending, false);
    fallback.recovery.mode = RecoveryViewMode::FallbackSelectionRequired;
    fallback.home.mode = FermentationHomeMode::Recovery;
    const auto fallbackView = workspace.view(fallback, &catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::ResumeFallback),
        static_cast<int>(fallbackView.slotActions[1]));
    TEST_ASSERT_TRUE(workspace.press(fallback, bottom(1), &catalog)
                         .resumeFallback.has_value());

    auto safeBoot = fallback;
    safeBoot.home.processState = ProcessState::SafeBoot;
    safeBoot.home.mode = FermentationHomeMode::Restricted;
    workspace.setPage(FermentationUiPage::Home);
    const auto safeBootView = workspace.view(safeBoot, &catalog);
    TEST_ASSERT_FALSE(safeBootView.bottomSlots[0].enabled);
    TEST_ASSERT_FALSE(safeBootView.bottomSlots[1].enabled);
    TEST_ASSERT_EQUAL_UINT32(4U, safeBootView.unavailableCapabilities.size());
    TEST_ASSERT_TRUE(
        std::find(safeBootView.unavailableCapabilities.begin(),
                  safeBootView.unavailableCapabilities.end(),
                  FermentationUiSafeBootCapability::PersistentFactoryReset) !=
        safeBootView.unavailableCapabilities.end());
    TEST_ASSERT_TRUE(
        std::find(safeBootView.unavailableCapabilities.begin(),
                  safeBootView.unavailableCapabilities.end(),
                  FermentationUiSafeBootCapability::RawTouchRecovery) !=
        safeBootView.unavailableCapabilities.end());
    TEST_ASSERT_TRUE(
        std::find(
            safeBootView.unavailableCapabilities.begin(),
            safeBootView.unavailableCapabilities.end(),
            FermentationUiSafeBootCapability::NetworkProvisioningRecovery) !=
        safeBootView.unavailableCapabilities.end());
    TEST_ASSERT_TRUE(
        std::find(safeBootView.unavailableCapabilities.begin(),
                  safeBootView.unavailableCapabilities.end(),
                  FermentationUiSafeBootCapability::DiagnosticsExport) !=
        safeBootView.unavailableCapabilities.end());
    workspace.setPage(FermentationUiPage::Recovery);
    const auto safeBootRecoveryView = workspace.view(safeBoot, &catalog);
    TEST_ASSERT_FALSE(safeBootRecoveryView.bottomSlots[1].enabled);

    workspace.setPage(FermentationUiPage::Home);
    const auto header = workspace.press(
        standby, {device_platform::DeviceUiTargetKind::HeaderLanguage, 0U});
    TEST_ASSERT_TRUE(header.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::HeaderLanguage),
                          static_cast<int>(workspace.page()));
    const auto network = workspace.press(
        standby, {device_platform::DeviceUiTargetKind::HeaderNetwork, 0U});
    TEST_ASSERT_TRUE(network.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::HeaderNetwork),
                          static_cast<int>(workspace.page()));
    const auto clock = workspace.press(
        standby, {device_platform::DeviceUiTargetKind::HeaderClock, 0U});
    TEST_ASSERT_TRUE(clock.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::HeaderClock),
                          static_cast<int>(workspace.page()));
}

// SIM-26-02, SIM-26-05 and SIM-26-13: navigation is a stack operation;
// Back/Cancel/Pager never borrow the Fachcommand in the same slot.
void test_sim_26_navigation_and_non_command_slots() {
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::ProgramEdit);
    auto back = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Back, 0U});
    TEST_ASSERT_TRUE(back.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramSummary),
                          static_cast<int>(workspace.page()));
    back = workspace.press(snapshot,
                           {device_platform::DeviceUiTargetKind::Cancel, 0U});
    TEST_ASSERT_TRUE(back.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramList),
                          static_cast<int>(workspace.page()));
    back = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::HomeOrBack, 0U});
    TEST_ASSERT_TRUE(back.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Home),
                          static_cast<int>(workspace.page()));

    workspace.setPage(FermentationUiPage::ProgramSummary);
    const auto summaryBack = workspace.press(snapshot, bottom(0));
    TEST_ASSERT_TRUE(summaryBack.navigated);
    TEST_ASSERT_FALSE(summaryBack.action.has_value());
    workspace.setPage(FermentationUiPage::Messages);
    auto messages = snapshotWithMessage(ProcessState::Fermenting,
                                        MessageCode::RunCompleted, false);
    messages.messages.push_back(messages.messages.front());
    const auto messageView = workspace.view(messages);
    TEST_ASSERT_TRUE(messageView.bottomSlots[2].enabled);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::MovePagerDown),
        static_cast<int>(messageView.slotActions[2]));
    const auto pager = workspace.press(messages, bottom(2));
    TEST_ASSERT_TRUE(pager.navigated);
    TEST_ASSERT_FALSE(pager.action.has_value());
    TEST_ASSERT_FALSE(pager.transitionAction.has_value());
}

// SIM-26-03, SIM-26-08, SIM-26-29, SIM-26-37, SIM-26-38, SIM-26-57 and
// SIM-26-66: manual modes and program start are reachable as separate
// existing payloads and never manufacture an identity in the workspace.
void test_sim_26_manual_and_program_consumer_paths() {
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    auto catalog = runnableCatalog();
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::ProgramList);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(3), &catalog).navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiPage::ManualModeSelection),
        static_cast<int>(workspace.page()));

    FermentationUiManualRunPlanValues holding;
    holding.targetTemperatureCelsius = 30.0;
    holding.qualificationBandCelsius = 0.5;
    holding.qualificationDurationMinutes = 10U;
    holding.maximumTargetReachMinutes = 180U;
    workspace.setManualHoldingValues(holding);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(1), &catalog).navigated);
    const auto holdingView = workspace.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(holdingView.bottomSlots[2].enabled);
    const auto holdingPress = workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_TRUE(holdingPress.action.has_value());
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiStartManualHoldingIntent>(
            *holdingPress.action));

    workspace.setPage(FermentationUiPage::ManualModeSelection);
    ManualTimedRunValues timed;
    timed.targetTemperatureCelsius = 30.0;
    timed.durationMinutes = 60U;
    timed.qualificationBandCelsius = 0.5;
    timed.qualificationDurationMinutes = 10U;
    timed.maximumTargetReachMinutes = 180U;
    workspace.setManualTimedValues(timed);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2), &catalog).navigated);
    const auto timedPress = workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_TRUE(timedPress.action.has_value());
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiStartManualTimedIntent>(
            *timedPress.action));

    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::LocalDisplay;
    context.expected.expectedStateSequence = 0U;
    const auto prepared =
        application.prepareEnvelope(context, *timedPress.action);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(prepared.status));
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_TRUE(prepared.request->runId().has_value());

    workspace.setPage(FermentationUiPage::Home);
    TEST_ASSERT_TRUE(
        workspace.selectProgram(catalog.programs.back().program.id, catalog));
    const auto summary = workspace.view(snapshot, &catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::StartProgram),
        static_cast<int>(summary.slotActions[2]));
    const auto start = workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_TRUE(start.action.has_value());
    TEST_ASSERT_TRUE(std::holds_alternative<FermentationUiStartProgramIntent>(
        *start.action));
    const auto& candidate =
        std::get<FermentationUiStartProgramIntent>(*start.action).candidate;
    TEST_ASSERT_EQUAL_STRING(catalog.programs.back().program.id.c_str(),
                             candidate.programId.c_str());
    auto overrideCandidate = candidate;
    overrideCandidate.targetTemperatureCelsius = 24.0;
    workspace.setStartCandidate(overrideCandidate);
    const auto overriddenStart = workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_TRUE(overriddenStart.action.has_value());
    TEST_ASSERT_TRUE(
        std::get<FermentationUiStartProgramIntent>(*overriddenStart.action)
            .candidate.targetTemperatureCelsius.has_value());
    overrideCandidate.programId = "other-program";
    workspace.setStartCandidate(overrideCandidate);
    const auto rejectedCandidate =
        workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_FALSE(rejectedCandidate.action.has_value());

    workspace.setPage(FermentationUiPage::ProgramList);
    const auto programList = workspace.view(snapshot, &catalog);
    TEST_ASSERT_EQUAL_UINT32(catalog.programs.size(),
                             programList.programList.size());
    TEST_ASSERT_TRUE(
        programList.programList.front().program.program.factoryCatalogEntry);
}

// SIM-26-18, SIM-26-24, SIM-26-26, SIM-26-45, SIM-26-46 and SIM-26-56:
// shell geometry, locale completeness and service/PIN state remain platform
// contracts.
void test_sim_26_shell_locale_and_service_boundaries() {
    device_platform_test_support::SimulatedDeviceShell shell;
    TEST_ASSERT_TRUE(shell.frame().valid());
    const auto packs = makeFermentationUiTextPacks();
    const std::array<const char*, 55U> keys{"standby",
                                            "running",
                                            "waiting",
                                            "completed",
                                            "restricted",
                                            "recovery",
                                            "unavailable",
                                            "start",
                                            "preheat",
                                            "programs",
                                            "status",
                                            "service",
                                            "stop",
                                            "details",
                                            "continue",
                                            "ok",
                                            "cool-now",
                                            "back",
                                            "home",
                                            "up",
                                            "down",
                                            "confirm",
                                            "cancel",
                                            "service-locked",
                                            "service-home-locked",
                                            "resume-fallback",
                                            "manual",
                                            "manual-holding",
                                            "manual-timed",
                                            "technical",
                                            "messages",
                                            "message-detail",
                                            "diagnostics",
                                            "pin",
                                            "language",
                                            "network",
                                            "clock",
                                            "program-actions",
                                            "program-edit",
                                            "edit",
                                            "copy",
                                            "new",
                                            "reset",
                                            "delete",
                                            "uninstall",
                                            "save",
                                            "stop-turn-off",
                                            "stop-and-cool",
                                            "acknowledge",
                                            "mute",
                                            "fault-reset",
                                            "program-not-installed",
                                            "program-disabled",
                                            "program-invalid",
                                            "factory-reset-required"};
    for (const auto& pack : packs) {
        for (const auto* value : keys) {
            const auto found =
                std::find_if(pack.translations.begin(), pack.translations.end(),
                             [value](const auto& entry) {
                                 return entry.key.value == value;
                             });
            TEST_ASSERT_TRUE(found != pack.translations.end());
        }
    }
    auto idle = device_platform::DeviceUiIdleController(1000U);
    idle.observeUserActivity(10U);
    idle.tick(1010U);
    const auto wake = shell.press(bottom(0), true, false, idle.isSleeping());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::WakeOnly),
        static_cast<int>(wake.outcome));
    idle.observeSystemWake();
    const auto selected =
        shell.press(bottom(0), true, false, idle.isSleeping());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiInteractionOutcome::TargetSelected),
        static_cast<int>(selected.outcome));

    auto service = device_platform::ServiceSessionLease(
        fermentationTouchServicePolicy(), 100U);
    TEST_ASSERT_TRUE(service.activeAt(100U));
    service.observe(device_platform::ServiceSessionEvent::RelevantUserActivity,
                    9U * 60U * 1000U);
    TEST_ASSERT_TRUE(service.activeAt(10U * 60U * 1000U));
    service.observe(device_platform::ServiceSessionEvent::DeviceRestart,
                    10U * 60U * 1000U);
    TEST_ASSERT_FALSE(service.activeAt(10U * 60U * 1000U));
    auto invalidated = device_platform::ServiceSessionLease(
        fermentationTouchServicePolicy(), 0U);
    invalidated.observe(
        device_platform::ServiceSessionEvent::SafetyStateInvalidated, 1U);
    TEST_ASSERT_FALSE(invalidated.activeAt(1U));
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
}

// SIM-26-41, SIM-26-42, SIM-26-43, SIM-26-44 and SIM-26-58: editor buttons
// yield the owning edit request, while the actual mutation remains with the
// catalog/ConfigurationService helper and its expected revision.
void test_sim_26_program_editor_actions_are_real_requests() {
    const auto noUsage =
        makeFermentationUiProgramUsageEvidence(RunCommandState{});
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    auto catalog = runnableCatalog();
    FermentationTouchWorkspace workspace;
    const auto selectedId = catalog.programs.back().program.id;
    TEST_ASSERT_TRUE(workspace.selectProgram(selectedId, catalog));
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(1), &catalog).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2), &catalog).navigated);
    auto editor = workspace.view(snapshot, &catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::SaveProgram),
        static_cast<int>(editor.slotActions[3]));
    auto copy = workspace.press(snapshot, bottom(3), &catalog);
    TEST_ASSERT_TRUE(copy.programEdit.has_value());
    TEST_ASSERT_TRUE(copy.programEdit->operation ==
                     FermentationUiProgramEditOperation::Copy);
    const auto copied = applyProgramEdit(catalog, *copy.programEdit, noUsage);
    TEST_ASSERT_TRUE(copied.status == FermentationUiProgramEditStatus::Applied);

    workspace.setPage(FermentationUiPage::ProgramActions);
    const auto newPage = workspace.press(snapshot, bottom(3), &catalog);
    TEST_ASSERT_TRUE(newPage.navigated);
    auto newCandidate = catalog.programs.back();
    newCandidate.program.name = "Local new program";
    workspace.setProgramEditCandidate(newCandidate);
    const auto newRequest = workspace.press(snapshot, bottom(3), &catalog);
    TEST_ASSERT_TRUE(newRequest.programEdit.has_value());
    TEST_ASSERT_TRUE(newRequest.programEdit->operation ==
                     FermentationUiProgramEditOperation::New);
    const auto created =
        applyProgramEdit(catalog, *newRequest.programEdit, noUsage);
    TEST_ASSERT_TRUE(created.status ==
                     FermentationUiProgramEditStatus::Applied);

    workspace.setPage(FermentationUiPage::ProgramEdit);
    workspace.setProgramEditOperation(FermentationUiProgramEditOperation::Edit);
    workspace.setProgramEditCandidate(catalog.programs.back());
    const auto saved = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Confirm, 0U}, &catalog);
    TEST_ASSERT_TRUE(saved.programEdit.has_value());
    TEST_ASSERT_TRUE(saved.programEdit->candidate.has_value());

    workspace.setPage(FermentationUiPage::ProgramEdit);
    workspace.setProgramEditDirty(true);
    workspace.setProgramEditCandidate(catalog.programs.back());
    const auto blockedBack = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Back, 0U}, &catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Blocked),
        static_cast<int>(blockedBack.interaction.outcome));
    const auto allowedConfirm = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Confirm, 0U}, &catalog);
    TEST_ASSERT_TRUE(
        allowedConfirm.interaction.outcome ==
        device_platform::DeviceUiInteractionOutcome::TargetSelected);

    workspace.setPage(FermentationUiPage::Home);
    TEST_ASSERT_TRUE(workspace.selectProgram("user-00", catalog));
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(1), &catalog).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(1), &catalog).navigated);
    const auto enterDeleteConfirmation = [&]() {
        const auto deleteOffer = workspace.press(snapshot, bottom(2), &catalog);
        TEST_ASSERT_TRUE(deleteOffer.navigated);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(FermentationUiPage::ProgramDeleteConfirmation),
            static_cast<int>(workspace.page()));
        const auto first = workspace.view(snapshot, &catalog);
        TEST_ASSERT_TRUE(first.confirmationProgramName.has_value());
        TEST_ASSERT_EQUAL_STRING("Wasserkefir copy",
                                 first.confirmationProgramName->c_str());
        TEST_ASSERT_FALSE(first.confirmationWarning.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(FermentationUiWorkspaceSlotAction::
                                 NavigateProgramDeleteFinalConfirmation),
            static_cast<int>(first.slotActions[1]));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateBack),
            static_cast<int>(first.slotActions[2]));
    };

    enterDeleteConfirmation();
    const auto firstCancel = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Cancel, 0U}, &catalog);
    TEST_ASSERT_TRUE(firstCancel.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramEdit),
                          static_cast<int>(workspace.page()));
    TEST_ASSERT_TRUE(std::any_of(
        catalog.programs.begin(), catalog.programs.end(),
        [](const auto& item) { return item.program.id == "user-01"; }));

    enterDeleteConfirmation();
    const auto firstConfirmed = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Confirm, 0U}, &catalog);
    TEST_ASSERT_TRUE(firstConfirmed.navigated);
    TEST_ASSERT_FALSE(firstConfirmed.programEdit.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiPage::ProgramDeleteFinalConfirmation),
        static_cast<int>(workspace.page()));
    const auto second = workspace.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(second.confirmationProgramName.has_value());
    TEST_ASSERT_EQUAL_STRING("Wasserkefir copy",
                             second.confirmationProgramName->c_str());
    TEST_ASSERT_FALSE(second.confirmationWarning.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::DeleteProgram),
        static_cast<int>(second.slotActions[3]));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateStatus),
        static_cast<int>(second.slotActions[2]));

    const auto secondCancel = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Cancel, 0U}, &catalog);
    TEST_ASSERT_TRUE(secondCancel.navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiPage::ProgramDeleteConfirmation),
        static_cast<int>(workspace.page()));
    TEST_ASSERT_FALSE(secondCancel.programEdit.has_value());

    const auto secondFirstConfirmed = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Confirm, 0U}, &catalog);
    TEST_ASSERT_TRUE(secondFirstConfirmed.navigated);
    const auto confirmedDelete = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Confirm, 0U}, &catalog);
    TEST_ASSERT_TRUE(confirmedDelete.programEdit.has_value());
    TEST_ASSERT_TRUE(confirmedDelete.programEdit->confirmed);
    const auto deleted =
        applyProgramEdit(catalog, *confirmedDelete.programEdit, noUsage);
    TEST_ASSERT_TRUE(deleted.status ==
                     FermentationUiProgramEditStatus::Applied);
}

// SIM-26-43: owning run-state evidence, not the UI request, protects both
// user-program deletion and standard-program deinstallation.
void test_sim_26_program_delete_owner_usage_gate() {
    auto userCatalog = runnableCatalog();
    auto user = userCatalog.programs.back();
    user.program.id = "user-in-use";
    user.program.name = "In-use user program";
    user.program.builtIn = false;
    user.program.factoryCatalogEntry = false;
    user.program.resettable = false;
    user.program.userDeletable = true;
    user.program.installed = true;
    userCatalog.programs.push_back(user);

    RunCommandState userRun;
    userRun.processState.state = ProcessState::Fermenting;
    const auto activeUser = ActiveRun::start(
        user, ProgramSourceKind::UserProgram, RunProgramSourceRevision{1U});
    TEST_ASSERT_TRUE(activeUser.has_value());
    userRun.activeProgramRun = std::move(*activeUser);
    const auto userUsage = makeFermentationUiProgramUsageEvidence(userRun);
    const auto userDelete =
        applyProgramEdit(userCatalog,
                         {FermentationUiProgramEditOperation::Delete,
                          "user-in-use", std::nullopt, std::nullopt, true},
                         userUsage);
    TEST_ASSERT_TRUE(userDelete.status ==
                     FermentationUiProgramEditStatus::NotAllowed);
    TEST_ASSERT_EQUAL_UINT32(5U, userCatalog.programs.size());

    auto standardCatalog = runnableCatalog();
    const auto standardId = standardCatalog.programs.back().program.id;
    RunCommandState standardRun;
    standardRun.processState.state = ProcessState::Fermenting;
    const auto activeStandard = ActiveRun::start(
        standardCatalog.programs.back(), ProgramSourceKind::FactoryCatalog,
        RunProgramSourceRevision{1U});
    TEST_ASSERT_TRUE(activeStandard.has_value());
    standardRun.activeProgramRun = std::move(*activeStandard);
    const auto standardUsage =
        makeFermentationUiProgramUsageEvidence(standardRun);
    const auto standardUninstall =
        applyProgramEdit(standardCatalog,
                         {FermentationUiProgramEditOperation::Uninstall,
                          standardId, std::nullopt, std::nullopt, true},
                         standardUsage);
    TEST_ASSERT_TRUE(standardUninstall.status ==
                     FermentationUiProgramEditStatus::NotAllowed);
    TEST_ASSERT_EQUAL_UINT32(4U, standardCatalog.programs.size());
    TEST_ASSERT_TRUE(standardCatalog.programs.back().program.installed);
}

// SIM-26-43: a standard-program deinstallation has the same two observable
// confirmations as user-program deletion and only then emits Uninstall.
void test_sim_26_standard_delete_uses_two_confirmations() {
    const auto noUsage =
        makeFermentationUiProgramUsageEvidence(RunCommandState{});
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    auto catalog = runnableCatalog();
    const auto standardId = catalog.programs.back().program.id;
    const auto standardName = catalog.programs.back().program.name;
    FermentationTouchWorkspace workspace;
    TEST_ASSERT_TRUE(workspace.selectProgram(standardId, catalog));
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(1), &catalog).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(1), &catalog).navigated);
    const auto editor = workspace.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(editor.slotActions[2] ==
                     FermentationUiWorkspaceSlotAction::UninstallProgram);
    TEST_ASSERT_EQUAL_STRING("delete",
                             editor.bottomSlots[2].label.value.c_str());

    const auto firstOffer = workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_TRUE(firstOffer.navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiPage::ProgramDeleteConfirmation),
        static_cast<int>(workspace.page()));
    const auto first = workspace.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(first.confirmationProgramName.has_value());
    TEST_ASSERT_EQUAL_STRING(standardName.c_str(),
                             first.confirmationProgramName->c_str());
    TEST_ASSERT_TRUE(first.confirmationWarning.has_value());
    TEST_ASSERT_EQUAL_STRING("factory-reset-required",
                             first.confirmationWarning->value.c_str());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::
                             NavigateProgramDeleteFinalConfirmation),
        static_cast<int>(first.slotActions[1]));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateBack),
        static_cast<int>(first.slotActions[2]));
    const auto repeatedDeleteTouch =
        workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_TRUE(repeatedDeleteTouch.navigated);
    TEST_ASSERT_FALSE(repeatedDeleteTouch.programEdit.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramEdit),
                          static_cast<int>(workspace.page()));
    const auto firstReopened = workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_TRUE(firstReopened.navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiPage::ProgramDeleteConfirmation),
        static_cast<int>(workspace.page()));

    const auto firstConfirmed = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Confirm, 0U}, &catalog);
    TEST_ASSERT_TRUE(firstConfirmed.navigated);
    TEST_ASSERT_FALSE(firstConfirmed.programEdit.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiPage::ProgramDeleteFinalConfirmation),
        static_cast<int>(workspace.page()));
    const auto second = workspace.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(second.confirmationProgramName.has_value());
    TEST_ASSERT_EQUAL_STRING(standardName.c_str(),
                             second.confirmationProgramName->c_str());
    TEST_ASSERT_TRUE(second.confirmationWarning.has_value());
    TEST_ASSERT_EQUAL_STRING("factory-reset-required",
                             second.confirmationWarning->value.c_str());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::UninstallProgram),
        static_cast<int>(second.slotActions[3]));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateStatus),
        static_cast<int>(second.slotActions[2]));

    const auto final = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Confirm, 0U}, &catalog);
    TEST_ASSERT_TRUE(final.programEdit.has_value());
    TEST_ASSERT_TRUE(final.programEdit->operation ==
                     FermentationUiProgramEditOperation::Uninstall);
    TEST_ASSERT_TRUE(final.programEdit->confirmed);
    const auto mutation =
        applyProgramEdit(catalog, *final.programEdit, noUsage);
    TEST_ASSERT_TRUE(mutation.status ==
                     FermentationUiProgramEditStatus::Applied);
    TEST_ASSERT_EQUAL_UINT32(4U, catalog.programs.size());
    TEST_ASSERT_TRUE(catalog.programs.back().program.id == standardId);
    TEST_ASSERT_FALSE(catalog.programs.back().program.installed);
    const auto activeList = makeFermentationUiProgramList(catalog);
    TEST_ASSERT_TRUE(std::none_of(
        activeList.begin(), activeList.end(), [&](const auto& entry) {
            return entry.program.program.id == standardId;
        }));
}

// SIM-26-11, SIM-26-28 and SIM-26-30: existing message, sensor and recovery
// intents leave the workspace as canonical payloads.
void test_sim_26_message_sensor_and_recovery_actions() {
    const auto snapshot = snapshotWithMessage(
        ProcessState::Fermenting, MessageCode::UserDecisionRequired, true);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::MessageDetail);
    workspace.setSelectedMessage(7U);
    const auto messageView = workspace.view(snapshot);
    TEST_ASSERT_TRUE(messageView.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(messageView.bottomSlots[2].enabled);
    const auto acknowledged = workspace.press(snapshot, bottom(1));
    TEST_ASSERT_TRUE(acknowledged.action.has_value());
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiAcknowledgeMessageIntent>(
            *acknowledged.action));
    const auto muted = workspace.press(snapshot, bottom(2));
    TEST_ASSERT_TRUE(muted.action.has_value());
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiMuteMessageIntent>(*muted.action));
    workspace.setSensorSelectionAction(
        SensorSelectionUserAction::ContinueWithAir);
    const auto sensor = workspace.press(snapshot, bottom(3));
    TEST_ASSERT_TRUE(sensor.action.has_value());
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiSensorSelectionIntent>(
            *sensor.action));

    workspace.setPage(FermentationUiPage::Recovery);
    workspace.setRecoveryTimeCorrectionSeconds(30U);
    const auto correction = workspace.press(snapshot, bottom(1));
    TEST_ASSERT_TRUE(correction.action.has_value());
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiRecoveryTimeCorrectionIntent>(
            *correction.action));
}

void test_network_page_exposes_only_the_two_modes_and_explicit_setup_action() {
    auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderNetwork);

    auto view = workspace.view(snapshot);
    TEST_ASSERT_TRUE(view.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(view.bottomSlots[2].enabled);
    TEST_ASSERT_FALSE(view.bottomSlots[3].enabled);
    const auto apOnly = workspace.press(snapshot, bottom(1));
    TEST_ASSERT_TRUE(apOnly.applyNetworkMode.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkMode::AP_ONLY),
        static_cast<int>(apOnly.applyNetworkMode->selectedMode));
    const auto homeWifi = workspace.press(snapshot, bottom(2));
    TEST_ASSERT_TRUE(homeWifi.applyNetworkMode.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkMode::HOME_WIFI),
        static_cast<int>(homeWifi.applyNetworkMode->selectedMode));

    snapshot.network.currentMode = device_platform::NetworkMode::AP_ONLY;
    view = workspace.view(snapshot);
    TEST_ASSERT_FALSE(view.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(view.bottomSlots[2].enabled);
    TEST_ASSERT_FALSE(view.bottomSlots[3].enabled);

    snapshot.network.currentMode = device_platform::NetworkMode::HOME_WIFI;
    view = workspace.view(snapshot);
    TEST_ASSERT_TRUE(view.bottomSlots[1].enabled);
    TEST_ASSERT_FALSE(view.bottomSlots[2].enabled);
    TEST_ASSERT_TRUE(view.bottomSlots[3].enabled);
    const auto reconfigure = workspace.press(snapshot, bottom(3));
    TEST_ASSERT_TRUE(reconfigure.beginHomeWifiReconfiguration.has_value());
}

void test_web_access_page_is_reachable_and_slot_follows_application_state() {
    auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderLanguage);

    // Existing header navigation is unchanged; the new entry is slot 3.
    auto view = workspace.view(snapshot);
    TEST_ASSERT_TRUE(view.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(view.bottomSlots[2].enabled);
    TEST_ASSERT_TRUE(view.bottomSlots[3].enabled);
    TEST_ASSERT_TRUE(view.slotActions[1] ==
                     FermentationUiWorkspaceSlotAction::NavigateNetwork);
    TEST_ASSERT_TRUE(view.slotActions[2] ==
                     FermentationUiWorkspaceSlotAction::NavigateClock);
    TEST_ASSERT_TRUE(view.slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::NavigateWebAccess);

    const auto entered = workspace.press(snapshot, bottom(3));
    TEST_ASSERT_TRUE(entered.navigated);
    TEST_ASSERT_FALSE(entered.openWebProvisioningWindow.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::HeaderWebAccess),
                          static_cast<int>(workspace.page()));
    view = workspace.view(snapshot);
    TEST_ASSERT_FALSE(view.route.segments.empty());
    TEST_ASSERT_TRUE(view.bottomSlots[0].enabled);
    TEST_ASSERT_TRUE(
        view.slotActions[1] ==
        FermentationUiWorkspaceSlotAction::OpenWebProvisioningWindow);

    // The slot mirrors the Application-reported state; the UI has no window.
    snapshot.webAccess = FermentationWebAccessState::NotApplicable;
    view = workspace.view(snapshot);
    TEST_ASSERT_FALSE(view.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(view.blockedReason.has_value());
    {
        const device_platform::TextKey expectedReason{
            device_platform::TextNamespace{"fermentation"},
            "web-access-unavailable"};
        TEST_ASSERT_TRUE(*view.blockedReason == expectedReason);
    }
    auto blocked = workspace.press(snapshot, bottom(1));
    TEST_ASSERT_FALSE(blocked.openWebProvisioningWindow.has_value());
    TEST_ASSERT_TRUE(blocked.interaction.outcome ==
                     device_platform::DeviceUiInteractionOutcome::Blocked);

    snapshot.webAccess = FermentationWebAccessState::WindowOpen;
    view = workspace.view(snapshot);
    TEST_ASSERT_FALSE(view.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(view.blockedReason.has_value());
    {
        const device_platform::TextKey expectedReason{
            device_platform::TextNamespace{"fermentation"},
            "web-access-window-open"};
        TEST_ASSERT_TRUE(*view.blockedReason == expectedReason);
    }
    blocked = workspace.press(snapshot, bottom(1));
    TEST_ASSERT_FALSE(blocked.openWebProvisioningWindow.has_value());

    snapshot.webAccess = FermentationWebAccessState::Closed;
    view = workspace.view(snapshot);
    TEST_ASSERT_TRUE(view.bottomSlots[1].enabled);
    TEST_ASSERT_FALSE(view.blockedReason.has_value());
    const auto opened = workspace.press(snapshot, bottom(1));
    TEST_ASSERT_TRUE(opened.openWebProvisioningWindow.has_value());
    TEST_ASSERT_FALSE(opened.navigated);

    // Back returns to the language page.
    const auto back = workspace.press(snapshot, bottom(0));
    TEST_ASSERT_TRUE(back.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::HeaderLanguage),
                          static_cast<int>(workspace.page()));
}

// S1: a catalog with enough installed programs to need the three-row window.
ProgramCatalog catalogWithPrograms(std::size_t userPrograms) {
    auto catalog = runnableCatalog();
    const auto templateDocument = catalog.programs.back();
    for (std::size_t index = 0U; index < userPrograms; ++index) {
        auto document = templateDocument;
        document.program.id = "user-" + std::to_string(index);
        document.program.name = "Program " + std::to_string(index);
        catalog.programs.push_back(std::move(document));
    }
    return catalog;
}

// S7: the ProgramSummary view carries the selected program's values with the
// next-run candidate overrides applied; a candidate of another program and
// pages other than ProgramSummary carry none.
void test_program_summary_view_applies_candidate_overrides_per_value() {
    auto catalog = catalogWithPrograms(1U);
    auto& program = catalog.programs.back().program;
    program.fermentationStages.front().targetTemperatureCelsius = 25.0;
    program.fermentationStages.front().durationMinutes = 60U;
    program.preheat = false;
    program.sensorPreference = SensorPreference::AirOnly;
    program.completion.mode = CompletionMode::FinishWithoutCooling;
    const auto id = program.id;
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;

    TEST_ASSERT_FALSE(
        workspace.view(snapshot, &catalog).programSummary.has_value());
    TEST_ASSERT_TRUE(workspace.selectProgram(id, catalog));
    auto summary = workspace.view(snapshot, &catalog).programSummary;
    TEST_ASSERT_TRUE(summary.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(25.0, *summary->targetTemperatureCelsius);
    TEST_ASSERT_EQUAL_UINT32(60U, *summary->durationMinutes);
    TEST_ASSERT_FALSE(summary->preheat);
    TEST_ASSERT_FALSE(summary->sensorModeOverride.has_value());
    TEST_ASSERT_TRUE(summary->sensorPreference == SensorPreference::AirOnly);

    // Only the overridden values change; the rest stays the program's.
    FermentationUiStartCandidate candidate;
    candidate.programId = id;
    candidate.targetTemperatureCelsius = 28.0;
    candidate.sensorMode = RunSensorMode::Product;
    workspace.setStartCandidate(candidate);
    summary = workspace.view(snapshot, &catalog).programSummary;
    TEST_ASSERT_EQUAL_DOUBLE(28.0, *summary->targetTemperatureCelsius);
    TEST_ASSERT_EQUAL_UINT32(60U, *summary->durationMinutes);
    TEST_ASSERT_TRUE(summary->sensorModeOverride ==
                     std::optional<RunSensorMode>{RunSensorMode::Product});

    // The summary is a display projection: the program itself is unchanged.
    TEST_ASSERT_EQUAL_DOUBLE(25.0, *catalog.programs.back()
                                        .program.fermentationStages.front()
                                        .targetTemperatureCelsius);

    candidate.programId = "other";
    workspace.setStartCandidate(candidate);
    summary = workspace.view(snapshot, &catalog).programSummary;
    TEST_ASSERT_EQUAL_DOUBLE(25.0, *summary->targetTemperatureCelsius);

    workspace.setPage(FermentationUiPage::Process);
    TEST_ASSERT_FALSE(
        workspace.view(snapshot, &catalog).programSummary.has_value());
}

// S7: the technical page pages over the snapshot temperatures.
void test_technical_page_pager_follows_the_snapshot_temperatures() {
    auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    snapshot.temperatures.resize(3U);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::Technical);
    auto view = workspace.view(snapshot);
    TEST_ASSERT_EQUAL_UINT32(3U, view.pager.itemCount);
    TEST_ASSERT_FALSE(view.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(view.bottomSlots[2].enabled);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2)).navigated);
    view = workspace.view(snapshot);
    TEST_ASSERT_EQUAL_UINT32(1U, view.pager.currentIndex);
    TEST_ASSERT_TRUE(view.bottomSlots[1].enabled);

    snapshot.temperatures.clear();
    workspace.setPage(FermentationUiPage::Home);
    workspace.setPage(FermentationUiPage::Technical);
    view = workspace.view(snapshot);
    TEST_ASSERT_EQUAL_UINT32(0U, view.pager.itemCount);
    TEST_ASSERT_FALSE(view.bottomSlots[2].enabled);
}

// S7 / plan 4.1: without a staged value (no production caller stages one)
// the Recovery page offers no time-correction slot, in any recovery mode.
void test_recovery_time_correction_is_never_offered_without_a_staged_value() {
    for (const auto mode :
         {RecoveryViewMode::Normal, RecoveryViewMode::WaitingForTrustedTime,
          RecoveryViewMode::CurrentRunRecovered,
          RecoveryViewMode::FallbackSelectionRequired,
          RecoveryViewMode::RecoveryRejectedOrFailClosed}) {
        auto snapshot =
            snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
        snapshot.recovery.mode = mode;
        FermentationTouchWorkspace workspace;
        workspace.setPage(FermentationUiPage::Recovery);
        const auto view = workspace.view(snapshot);
        for (const auto action : view.slotActions) {
            TEST_ASSERT_TRUE(
                action !=
                FermentationUiWorkspaceSlotAction::ApplyRecoveryTimeCorrection);
        }
    }
}

device_platform::DeviceUiTarget cell(std::uint8_t row) {
    return {device_platform::DeviceUiTargetKind::ContentCell, 0U, row, 0U};
}

void test_program_list_cell_selects_the_row_of_the_visible_window() {
    const auto catalog = catalogWithPrograms(4U);
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::ProgramList);
    const auto list = workspace.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(list.programList.size() >= 5U);

    // Row 1 of the unscrolled window is entry 1; the press carries no payload.
    const auto first = workspace.press(snapshot, cell(1U), &catalog);
    TEST_ASSERT_TRUE(first.navigated);
    TEST_ASSERT_FALSE(first.action.has_value());
    TEST_ASSERT_FALSE(first.transitionAction.has_value());
    TEST_ASSERT_FALSE(first.programEdit.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramSummary),
                          static_cast<int>(workspace.page()));
    TEST_ASSERT_EQUAL_STRING(list.programList[1].program.program.id.c_str(),
                             workspace.selectedProgramId()->c_str());

    // Scrolling by two moves the window: row 2 is now entry 4.
    workspace.setPage(FermentationUiPage::ProgramList);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2), &catalog).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2), &catalog).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, cell(2U), &catalog).navigated);
    TEST_ASSERT_EQUAL_STRING(list.programList[4].program.program.id.c_str(),
                             workspace.selectedProgramId()->c_str());
}

void test_program_list_cell_outside_the_window_or_page_is_blocked() {
    const auto catalog = catalogWithPrograms(1U);
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::ProgramList);
    const auto count = workspace.view(snapshot, &catalog).programList.size();
    TEST_ASSERT_TRUE(count >= 2U);

    // Scrolled to the last entry only row 0 is backed by an entry.
    for (std::size_t index = 0U; index + 1U < count; ++index)
        TEST_ASSERT_TRUE(
            workspace.press(snapshot, bottom(2), &catalog).navigated);
    const auto beyond = workspace.press(snapshot, cell(1U), &catalog);
    TEST_ASSERT_FALSE(beyond.navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Blocked),
        static_cast<int>(beyond.interaction.outcome));
    const auto tooHigh = workspace.press(snapshot, cell(3U), &catalog);
    TEST_ASSERT_FALSE(tooHigh.navigated);
    const auto wrongColumn = workspace.press(
        snapshot,
        {device_platform::DeviceUiTargetKind::ContentCell, 0U, 0U, 1U},
        &catalog);
    TEST_ASSERT_FALSE(wrongColumn.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramList),
                          static_cast<int>(workspace.page()));

    // Without a catalog there is no list and no hittable row.
    FermentationTouchWorkspace noCatalog;
    noCatalog.setPage(FermentationUiPage::ProgramList);
    TEST_ASSERT_FALSE(noCatalog.press(snapshot, cell(0U)).navigated);
    // A content cell never acts on a page without a list.
    FermentationTouchWorkspace home;
    TEST_ASSERT_FALSE(home.press(snapshot, cell(0U), &catalog).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Home),
                          static_cast<int>(home.page()));
}

// D12: a listed but not startable program stays selectable (administration),
// shows the owning reason and never becomes the start candidate.
void test_unstartable_program_is_selectable_with_reason_and_no_start() {
    auto catalog = catalogWithPrograms(1U);
    catalog.programs.back().program.enabled = false;
    const auto id = catalog.programs.back().program.id;
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;

    TEST_ASSERT_TRUE(workspace.selectProgram(id, catalog));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramSummary),
                          static_cast<int>(workspace.page()));
    const auto summary = workspace.view(snapshot, &catalog);
    TEST_ASSERT_FALSE(summary.bottomSlots[2].enabled);
    TEST_ASSERT_TRUE(summary.blockedReason.has_value());
    TEST_ASSERT_TRUE(*summary.blockedReason ==
                     fermentationTextKey("program-disabled"));
    // edit stays reachable for administration.
    TEST_ASSERT_TRUE(summary.bottomSlots[1].enabled);
    // A forced confirm press yields no start payload.
    const auto confirm = workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_FALSE(confirm.action.has_value());

    // A startable program has no reason and enables confirm.
    const auto startableId =
        catalog.programs[catalog.programs.size() - 2U].program.id;
    TEST_ASSERT_TRUE(workspace.selectProgram(startableId, catalog));
    const auto startable = workspace.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(startable.bottomSlots[2].enabled);
    TEST_ASSERT_FALSE(startable.blockedReason.has_value());
    // Unknown ids are still rejected.
    TEST_ASSERT_FALSE(workspace.selectProgram("does-not-exist", catalog));
}

// D14: Start and Programme open the same list with a different intent.
void test_start_and_programs_open_the_list_with_separate_intent() {
    const auto catalog = catalogWithPrograms(1U);
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);

    FermentationTouchWorkspace start;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            FermentationUiWorkspaceSlotAction::NavigateProgramList),
        static_cast<int>(start.view(snapshot, &catalog).slotActions[0]));
    TEST_ASSERT_TRUE(start.press(snapshot, bottom(0), &catalog).navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiProgramListIntent::Start),
        static_cast<int>(start.programListIntent()));
    const auto startList = start.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(startList.title == fermentationTextKey("start"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            FermentationUiWorkspaceSlotAction::NavigateManualModeSelection),
        static_cast<int>(startList.slotActions[3]));
    TEST_ASSERT_TRUE(start.press(snapshot, cell(0U), &catalog).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramSummary),
                          static_cast<int>(start.page()));

    FermentationTouchWorkspace manage;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            FermentationUiWorkspaceSlotAction::NavigateProgramManagement),
        static_cast<int>(manage.view(snapshot, &catalog).slotActions[1]));
    TEST_ASSERT_TRUE(manage.press(snapshot, bottom(1), &catalog).navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiProgramListIntent::Manage),
        static_cast<int>(manage.programListIntent()));
    const auto manageList = manage.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(manageList.title == fermentationTextKey("programs"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateStatus),
        static_cast<int>(manageList.slotActions[3]));
    const auto picked = manage.press(snapshot, cell(1U), &catalog);
    TEST_ASSERT_TRUE(picked.navigated);
    TEST_ASSERT_FALSE(picked.action.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramActions),
                          static_cast<int>(manage.page()));
    TEST_ASSERT_EQUAL_STRING(
        manageList.programList[1].program.program.id.c_str(),
        manage.selectedProgramId()->c_str());
    const auto actions = manage.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(actions.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(actions.bottomSlots[2].enabled);
    // Back returns to the management list, not to the start path.
    TEST_ASSERT_TRUE(manage.press(snapshot, bottom(0), &catalog).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramList),
                          static_cast<int>(manage.page()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiProgramListIntent::Manage),
        static_cast<int>(manage.programListIntent()));

    // Opening Start afterwards resets the intent.
    manage.setPage(FermentationUiPage::Home);
    TEST_ASSERT_TRUE(manage.press(snapshot, bottom(0), &catalog).navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiProgramListIntent::Start),
        static_cast<int>(manage.programListIntent()));
}

// Review B2: an empty active list is a valid catalog; the management list must
// still reach `new`, while the start list keeps its manual path.
void test_manage_list_reaches_new_program_when_the_active_list_is_empty() {
    auto catalog = makeFactoryProgramCatalog();
    for (auto& document : catalog.programs) document.program.installed = false;
    TEST_ASSERT_EQUAL_UINT32(
        4U, static_cast<std::uint32_t>(catalog.programs.size()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProgramCatalogStatus::Success),
                          static_cast<int>(validateProgramCatalog(catalog)));
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);

    FermentationTouchWorkspace manage;
    TEST_ASSERT_TRUE(manage.press(snapshot, bottom(1), &catalog).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramList),
                          static_cast<int>(manage.page()));
    const auto list = manage.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(list.programList.empty());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NewProgram),
        static_cast<int>(list.slotActions[3]));
    TEST_ASSERT_TRUE(list.bottomSlots[3].enabled);
    // No row exists to hit.
    TEST_ASSERT_FALSE(manage.press(snapshot, cell(0U), &catalog).navigated);

    // `new` takes the existing NewProgram semantics into the editor path and
    // carries no application payload (no program/config owner is called).
    const auto created = manage.press(snapshot, bottom(3), &catalog);
    TEST_ASSERT_TRUE(created.navigated);
    TEST_ASSERT_FALSE(created.action.has_value());
    TEST_ASSERT_FALSE(created.programEdit.has_value());
    TEST_ASSERT_FALSE(created.transitionAction.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramEdit),
                          static_cast<int>(manage.page()));
    TEST_ASSERT_FALSE(manage.selectedProgramId().has_value());

    // The start intent keeps the manual path on an empty list.
    FermentationTouchWorkspace start;
    TEST_ASSERT_TRUE(start.press(snapshot, bottom(0), &catalog).navigated);
    const auto startList = start.view(snapshot, &catalog);
    TEST_ASSERT_TRUE(startList.programList.empty());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            FermentationUiWorkspaceSlotAction::NavigateManualModeSelection),
        static_cast<int>(startList.slotActions[3]));

    // A non-empty management list keeps the default status slot.
    const auto filled = catalogWithPrograms(1U);
    FermentationTouchWorkspace nonEmpty;
    TEST_ASSERT_TRUE(nonEmpty.press(snapshot, bottom(1), &filled).navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateStatus),
        static_cast<int>(nonEmpty.view(snapshot, &filled).slotActions[3]));
    // Before a catalog is available nothing is offered.
    FermentationTouchWorkspace noCatalog;
    TEST_ASSERT_TRUE(noCatalog.press(snapshot, bottom(1)).navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateStatus),
        static_cast<int>(noCatalog.view(snapshot).slotActions[3]));
}

// S2: a snapshot with several messages (ids 11..15, the first one decision
// required) for the list window.
FermentationUiSnapshot snapshotWithMessages(std::size_t count) {
    auto snapshot = snapshotWithMessage(ProcessState::Fermenting,
                                        MessageCode::RunCompleted, false);
    snapshot.messages.clear();
    for (std::size_t index = 0U; index < count; ++index) {
        RuntimeMessage message;
        message.id = static_cast<std::uint32_t>(11U + index);
        message.code = MessageCode::RunCompleted;
        message.active = true;
        message.decisionRequired = index == 0U;
        snapshot.messages.push_back({message});
    }
    return snapshot;
}

void test_message_list_cell_selects_the_canonical_message_id() {
    const auto snapshot = snapshotWithMessages(5U);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::Messages);

    // The details slot needs an explicit selection and never picks one.
    TEST_ASSERT_FALSE(workspace.view(snapshot).bottomSlots[3].enabled);

    const auto first = workspace.press(snapshot, cell(1U));
    TEST_ASSERT_TRUE(first.navigated);
    TEST_ASSERT_FALSE(first.action.has_value());
    TEST_ASSERT_FALSE(first.transitionAction.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::MessageDetail),
                          static_cast<int>(workspace.page()));
    TEST_ASSERT_EQUAL_UINT32(12U, *workspace.view(snapshot).selectedMessageId);

    // Back to the list: the explicit selection (not the first decision) opens.
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(0)).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Messages),
                          static_cast<int>(workspace.page()));
    TEST_ASSERT_TRUE(workspace.view(snapshot).bottomSlots[3].enabled);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(3)).navigated);
    TEST_ASSERT_EQUAL_UINT32(12U, *workspace.view(snapshot).selectedMessageId);

    // Scrolled by two, row 2 is the fifth message.
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(0)).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2)).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2)).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, cell(2U)).navigated);
    TEST_ASSERT_EQUAL_UINT32(15U, *workspace.view(snapshot).selectedMessageId);

    // Ack and mute carry the selected canonical id; slot 3 stays unchanged.
    const auto acknowledged = workspace.press(snapshot, bottom(1));
    TEST_ASSERT_TRUE(acknowledged.action.has_value());
    const auto* ack = std::get_if<FermentationUiAcknowledgeMessageIntent>(
        &*acknowledged.action);
    TEST_ASSERT_TRUE(ack != nullptr);
    TEST_ASSERT_EQUAL_UINT32(15U, ack->messageId);
    const auto muted = workspace.press(snapshot, bottom(2));
    const auto* mute =
        std::get_if<FermentationUiMuteMessageIntent>(&*muted.action);
    TEST_ASSERT_TRUE(mute != nullptr);
    TEST_ASSERT_EQUAL_UINT32(15U, mute->messageId);
    const auto detail = workspace.view(snapshot);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::ResetFault),
        static_cast<int>(detail.slotActions[3]));
    TEST_ASSERT_FALSE(detail.bottomSlots[3].enabled);
}

void test_message_list_cell_outside_the_window_or_page_is_blocked() {
    const auto snapshot = snapshotWithMessages(2U);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::Messages);
    for (const std::uint8_t row : {std::uint8_t{2U}, std::uint8_t{3U}}) {
        const auto beyond = workspace.press(snapshot, cell(row));
        TEST_ASSERT_FALSE(beyond.navigated);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                device_platform::DeviceUiInteractionOutcome::Blocked),
            static_cast<int>(beyond.interaction.outcome));
    }
    TEST_ASSERT_FALSE(
        workspace
            .press(snapshot, {device_platform::DeviceUiTargetKind::ContentCell,
                              0U, 0U, 1U})
            .navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Messages),
                          static_cast<int>(workspace.page()));
    // No messages: no hittable row at all.
    FermentationTouchWorkspace empty;
    empty.setPage(FermentationUiPage::Messages);
    TEST_ASSERT_FALSE(
        empty.press(snapshotWithMessages(0U), cell(0U)).navigated);
    // A content cell never selects a message on another page.
    FermentationTouchWorkspace status;
    status.setPage(FermentationUiPage::Status);
    TEST_ASSERT_FALSE(status.press(snapshot, cell(0U)).navigated);
}

// Review B1: the waiting home opens the canonical decision-required message,
// not an earlier active, unresolved message of another kind.
void test_waiting_home_opens_the_canonical_decision_message_not_an_earlier_one() {
    auto snapshot = snapshotWithMessage(
        ProcessState::Fermenting, MessageCode::UserDecisionRequired, true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::Waiting),
                          static_cast<int>(snapshot.home.mode));
    RuntimeMessage earlier;
    earlier.id = 3U;
    earlier.code = MessageCode::RunCompleted;
    earlier.messageClass = MessageClass::Information;
    earlier.active = true;
    earlier.resolved = false;
    snapshot.messages.insert(snapshot.messages.begin(), {earlier});
    FermentationTouchWorkspace workspace;
    const auto home = workspace.view(snapshot);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            FermentationUiWorkspaceSlotAction::NavigateMessageDetail),
        static_cast<int>(home.slotActions[0]));
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(0)).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::MessageDetail),
                          static_cast<int>(workspace.page()));
    TEST_ASSERT_EQUAL_UINT32(7U, *workspace.view(snapshot).selectedMessageId);
}

// Review B2: a selection that left the snapshot keeps no active actions.
void test_stale_message_selection_offers_and_creates_no_message_action() {
    const auto first = snapshotWithMessages(5U);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::Messages);
    TEST_ASSERT_TRUE(workspace.press(first, cell(1U)).navigated);
    TEST_ASSERT_TRUE(workspace.view(first).bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(workspace.view(first).bottomSlots[2].enabled);

    // Same workspace, new snapshot without message id 12.
    auto later = snapshotWithMessages(5U);
    later.messages.erase(later.messages.begin() + 1);
    const auto stale = workspace.view(later);
    TEST_ASSERT_FALSE(stale.selectedMessageId.has_value());
    TEST_ASSERT_FALSE(stale.bottomSlots[1].enabled);
    TEST_ASSERT_FALSE(stale.bottomSlots[2].enabled);
    TEST_ASSERT_FALSE(workspace.press(later, bottom(1)).action.has_value());
    TEST_ASSERT_FALSE(workspace.press(later, bottom(2)).action.has_value());
    // Back on the list the details slot is disabled as well.
    TEST_ASSERT_TRUE(workspace.press(later, bottom(0)).navigated);
    TEST_ASSERT_FALSE(workspace.view(later).bottomSlots[3].enabled);
    // An empty snapshot behaves the same.
    workspace.setPage(FermentationUiPage::MessageDetail);
    const auto none = snapshotWithMessages(0U);
    TEST_ASSERT_FALSE(workspace.view(none).selectedMessageId.has_value());
    TEST_ASSERT_FALSE(workspace.press(none, bottom(1)).action.has_value());
}

// S3: the language page offers one row per build language and only issues the
// typed intent; the Application owns the change.
void test_language_page_rows_issue_a_language_intent_and_keep_the_slots() {
    auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    snapshot.revisions.expectedUserConfigurationRevision =
        UserConfigurationRevision{7U};
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderLanguage);
    const auto view = workspace.view(snapshot);
    TEST_ASSERT_EQUAL_UINT32(3U,
                             static_cast<std::uint32_t>(view.pager.itemCount));
    // Slots 1..3 stay as before (network, clock, the provisional #170
    // web access entry).
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateNetwork),
        static_cast<int>(view.slotActions[1]));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateClock),
        static_cast<int>(view.slotActions[2]));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateWebAccess),
        static_cast<int>(view.slotActions[3]));

    const std::array<const char*, 3U> expected{"de", "en", "es"};
    for (std::uint8_t row = 0U; row < expected.size(); ++row) {
        const auto press = workspace.press(snapshot, cell(row));
        TEST_ASSERT_TRUE(press.setDisplayLanguage.has_value());
        TEST_ASSERT_EQUAL_STRING(expected[row],
                                 press.setDisplayLanguage->languageId.c_str());
        TEST_ASSERT_TRUE(
            press.setDisplayLanguage->expectedUserConfigurationRevision ==
            snapshot.revisions.expectedUserConfigurationRevision);
        TEST_ASSERT_FALSE(press.navigated);
        TEST_ASSERT_FALSE(press.action.has_value());
        TEST_ASSERT_FALSE(press.transitionAction.has_value());
        TEST_ASSERT_FALSE(press.programEdit.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(FermentationUiPage::HeaderLanguage),
            static_cast<int>(workspace.page()));
    }
    // No row beyond the build catalog and no second column.
    const auto beyond = workspace.press(snapshot, cell(3U));
    TEST_ASSERT_FALSE(beyond.setDisplayLanguage.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Blocked),
        static_cast<int>(beyond.interaction.outcome));
    TEST_ASSERT_FALSE(
        workspace
            .press(snapshot, {device_platform::DeviceUiTargetKind::ContentCell,
                              0U, 0U, 1U})
            .setDisplayLanguage.has_value());

    // An undecidable revision is carried as absent (the Application rejects).
    snapshot.revisions.expectedUserConfigurationRevision.reset();
    const auto undecidable = workspace.press(snapshot, cell(0U));
    TEST_ASSERT_TRUE(undecidable.setDisplayLanguage.has_value());
    TEST_ASSERT_FALSE(undecidable.setDisplayLanguage
                          ->expectedUserConfigurationRevision.has_value());

    // The slot press path is unchanged: slot 3 still opens the web access page.
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(3)).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::HeaderWebAccess),
                          static_cast<int>(workspace.page()));
}

// Review B1: the refused-change note is transient display state of the
// language page only.
void test_language_failure_note_is_transient_and_page_local() {
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderLanguage);
    TEST_ASSERT_FALSE(workspace.view(snapshot).blockedReason.has_value());
    workspace.noteDisplayLanguageOutcome(false);
    const auto failed = workspace.view(snapshot);
    TEST_ASSERT_TRUE(failed.blockedReason.has_value());
    TEST_ASSERT_TRUE(*failed.blockedReason ==
                     fermentationTextKey("language-change-failed"));
    // The language slots and rows are unaffected by the note.
    TEST_ASSERT_TRUE(failed.bottomSlots[3].enabled);
    // An accepted outcome replaces it.
    workspace.noteDisplayLanguageOutcome(true);
    TEST_ASSERT_FALSE(workspace.view(snapshot).blockedReason.has_value());
    // Navigating away (slot) and back discards it.
    workspace.noteDisplayLanguageOutcome(false);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(1)).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(0)).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::HeaderLanguage),
                          static_cast<int>(workspace.page()));
    TEST_ASSERT_FALSE(workspace.view(snapshot).blockedReason.has_value());
    // The note never shows on other pages.
    workspace.noteDisplayLanguageOutcome(false);
    workspace.setPage(FermentationUiPage::HeaderNetwork);
    TEST_ASSERT_FALSE(workspace.view(snapshot).blockedReason.has_value());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_program_list_cell_selects_the_row_of_the_visible_window);
    RUN_TEST(test_program_list_cell_outside_the_window_or_page_is_blocked);
    RUN_TEST(test_unstartable_program_is_selectable_with_reason_and_no_start);
    RUN_TEST(test_start_and_programs_open_the_list_with_separate_intent);
    RUN_TEST(
        test_manage_list_reaches_new_program_when_the_active_list_is_empty);
    RUN_TEST(
        test_language_page_rows_issue_a_language_intent_and_keep_the_slots);
    RUN_TEST(test_language_failure_note_is_transient_and_page_local);
    RUN_TEST(test_message_list_cell_selects_the_canonical_message_id);
    RUN_TEST(test_message_list_cell_outside_the_window_or_page_is_blocked);
    RUN_TEST(
        test_waiting_home_opens_the_canonical_decision_message_not_an_earlier_one);
    RUN_TEST(test_stale_message_selection_offers_and_creates_no_message_action);
    RUN_TEST(
        test_web_access_page_is_reachable_and_slot_follows_application_state);
    RUN_TEST(test_workspace_has_fixed_slots_and_manual_paths_are_separate);
    RUN_TEST(test_workspace_navigation_does_not_create_a_command_id);
    RUN_TEST(test_active_home_press_opens_choice_without_preparing_a_command);
    RUN_TEST(test_shell_wake_is_first_touch_and_frame_is_deterministic);
    RUN_TEST(test_waiting_exposes_transition_intent_without_local_revision);
    RUN_TEST(test_pin_model_is_masked_and_owner_states_are_display_only);
    RUN_TEST(test_sim_26_workspace_action_matrix_and_owner_paths);
    RUN_TEST(test_sim_26_navigation_and_non_command_slots);
    RUN_TEST(test_sim_26_manual_and_program_consumer_paths);
    RUN_TEST(test_sim_26_shell_locale_and_service_boundaries);
    RUN_TEST(test_sim_26_program_editor_actions_are_real_requests);
    RUN_TEST(test_sim_26_program_delete_owner_usage_gate);
    RUN_TEST(test_sim_26_standard_delete_uses_two_confirmations);
    RUN_TEST(test_sim_26_message_sensor_and_recovery_actions);
    RUN_TEST(
        test_network_page_exposes_only_the_two_modes_and_explicit_setup_action);
    RUN_TEST(test_program_summary_view_applies_candidate_overrides_per_value);
    RUN_TEST(test_technical_page_pager_follows_the_snapshot_temperatures);
    RUN_TEST(
        test_recovery_time_correction_is_never_offered_without_a_staged_value);
    return UNITY_END();
}
