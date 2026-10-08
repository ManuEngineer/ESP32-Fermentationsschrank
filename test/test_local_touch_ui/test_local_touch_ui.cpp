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
    // Standby slot 3 is `Einstellungen` (O1/D14); Service lives below it.
    TEST_ASSERT_TRUE(home.bottomSlots[3].enabled);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateSettings),
        static_cast<int>(home.slotActions[3]));
    TEST_ASSERT_FALSE(home.blockedReason.has_value());
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
    // O5: no owner of the technical run limits exists, so the cooling start
    // stays disabled even with a staged plan.
    TEST_ASSERT_FALSE(completionView.bottomSlots[3].enabled);
    const auto cool = workspace.press(completed, bottom(3), &catalog);
    TEST_ASSERT_FALSE(cool.action.has_value());
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
    // O5: the manual start is disabled while no owner of the technical run
    // limits exists; the page names the reason.
    TEST_ASSERT_FALSE(holdingView.bottomSlots[2].enabled);
    TEST_ASSERT_TRUE(holdingView.blockedReason ==
                     fermentationTextKey("manual-parameters-not-released"));
    TEST_ASSERT_FALSE(
        workspace.press(snapshot, bottom(2), &catalog).action.has_value());

    workspace.setPage(FermentationUiPage::ManualModeSelection);
    ManualTimedRunValues timed;
    timed.targetTemperatureCelsius = 30.0;
    timed.durationMinutes = 60U;
    timed.qualificationBandCelsius = 0.5;
    timed.qualificationDurationMinutes = 10U;
    timed.maximumTargetReachMinutes = 180U;
    workspace.setManualTimedValues(timed);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2), &catalog).navigated);
    TEST_ASSERT_FALSE(
        workspace.press(snapshot, bottom(2), &catalog).action.has_value());
    const auto timedPayload = FermentationUiEnvelopePayload{
        FermentationUiStartManualTimedIntent{timed}};

    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::LocalDisplay;
    context.expected.expectedStateSequence = 0U;
    // The Application refuses the same payload too (no run identity used).
    const auto prepared = application.prepareEnvelope(context, timedPayload);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Unavailable),
        static_cast<int>(prepared.status));
    TEST_ASSERT_FALSE(prepared.request.has_value());

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
            const auto found = std::find_if(
                pack.translations.begin(), pack.translations.end(),
                [value](const auto& entry) { return entry.key == value; });
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

    // The editor works on the selected stored program; its candidate is a
    // valid edited copy (the save needs the existing program validation).
    TEST_ASSERT_TRUE(
        workspace.selectProgram(catalog.programs.back().program.id, catalog));
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
    // The time correction has no R1 user path: a staged test value opens no
    // press path either (see the dedicated regression test below).
    workspace.setRecoveryTimeCorrectionSeconds(30U);
    TEST_ASSERT_FALSE(workspace.press(snapshot, bottom(1)).action.has_value());
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
    workspace.setPage(FermentationUiPage::Settings);

    // The entry is the `Webzugang` row of the settings page (D13): scroll to
    // the window Device name / Network / Web access and tap the third row.
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2)).navigated);
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(2)).navigated);
    auto view = workspace.view(snapshot);
    TEST_ASSERT_TRUE(view.slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::NavigateStatus);
    const auto entered = workspace.press(
        snapshot,
        {device_platform::DeviceUiTargetKind::ContentCell, 0U, 2U, 0U});
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

    // Back returns to the settings page it was entered from.
    const auto back = workspace.press(snapshot, bottom(0));
    TEST_ASSERT_TRUE(back.navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Settings),
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

// S7 / plan 4.1: the Recovery page never offers a time-correction slot, in
// any recovery mode, also not with a value staged by the test helper.
void test_recovery_time_correction_is_never_offered_even_with_a_staged_value() {
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
        workspace.setRecoveryTimeCorrectionSeconds(30U);
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
    // Slots 1 and 2 stay (network, clock); the provisional #170 web access
    // slot 3 moved to the settings page (D13).
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateNetwork),
        static_cast<int>(view.slotActions[1]));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiWorkspaceSlotAction::NavigateClock),
        static_cast<int>(view.slotActions[2]));
    TEST_ASSERT_TRUE(view.slotActions[3] !=
                     FermentationUiWorkspaceSlotAction::NavigateWebAccess);

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

    // Slot 3 is the common status slot now; the web access entry is the
    // `Webzugang` row of the settings page.
    TEST_ASSERT_TRUE(workspace.press(snapshot, bottom(3)).navigated);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Status),
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

// S8: start values on ProgramSummary (next-run overrides only).
device_platform::DeviceUiTarget cellAt(std::uint8_t row, std::uint8_t column) {
    return {device_platform::DeviceUiTargetKind::ContentCell, 0U, row, column};
}

struct StartValueFixture {
    ProgramCatalog catalog;
    FermentationUiSnapshot snapshot;
    FermentationTouchWorkspace workspace;
    std::string id;

    StartValueFixture()
        : catalog(catalogWithPrograms(1U)),
          snapshot(snapshotFor(ProcessState::Standby,
                               FermentationHomeMode::Standby)) {
        auto& program = catalog.programs.back().program;
        program.completion.mode = CompletionMode::FinishWithoutCooling;
        program.completion.coolingTargetCelsius.reset();
        program.completion.holdDurationMinutes.reset();
        program.sensorPreference = SensorPreference::AirProductOptional;
        id = program.id;
        TEST_ASSERT_TRUE(workspace.selectProgram(id, catalog));
    }

    FermentationUiWorkspaceView view() const {
        return workspace.view(snapshot, &catalog);
    }
    FermentationUiWorkspacePress tap(std::uint8_t row, std::uint8_t column) {
        return workspace.press(snapshot, cellAt(row, column), &catalog);
    }
    FermentationUiWorkspacePress slot(std::uint8_t index) {
        return workspace.press(snapshot, bottom(index), &catalog);
    }
    // Types characters on the keypad: digits, `.` and `-` (sign).
    void type(const char* characters) {
        for (const char* c = characters; *c != '\0'; ++c) {
            std::uint8_t row = 3U;
            std::uint8_t column = 1U;
            if (*c >= '1' && *c <= '9') {
                row = static_cast<std::uint8_t>((*c - '1') / 3);
                column = static_cast<std::uint8_t>((*c - '1') % 3);
            } else if (*c == '.') {
                column = 0U;
            } else if (*c == '-') {
                column = 2U;
            }
            TEST_ASSERT_TRUE(tap(row, column).navigated);
        }
    }
    // The pager buttons are the ContentCells of column 1 (row 0 up, row 1
    // down).
    void scrollTo(std::size_t index) {
        while (view().pager.currentIndex < index) {
            TEST_ASSERT_TRUE(tap(1U, 1U).navigated);
        }
        while (view().pager.currentIndex > index) {
            TEST_ASSERT_TRUE(tap(0U, 1U).navigated);
        }
    }
};

void test_start_value_keypad_edit_sets_only_the_next_run_candidate() {
    StartValueFixture fixture;
    const auto stored = fixture.catalog.programs.back();
    auto summary = *fixture.view().programSummary;
    TEST_ASSERT_TRUE(summary.editable);
    TEST_ASSERT_TRUE(summary.fields[0] ==
                     FermentationUiStartField::TargetTemperature);
    // The unchanged state offers Status, not Reset (O8 a).
    TEST_ASSERT_TRUE(fixture.view().slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::NavigateStatus);

    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::ValueEdit);
    TEST_ASSERT_EQUAL_STRING("25.0",
                             fixture.view().valueEdit->candidate.c_str());
    // Slots: Cancel | Backspace | Clear | Commit.
    const auto edit = fixture.view();
    TEST_ASSERT_TRUE(edit.slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::ValueEditCancel);
    TEST_ASSERT_TRUE(edit.slotActions[1] ==
                     FermentationUiWorkspaceSlotAction::ValueEditBackspace);
    TEST_ASSERT_TRUE(edit.slotActions[2] ==
                     FermentationUiWorkspaceSlotAction::ValueEditClear);
    TEST_ASSERT_TRUE(edit.slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::ValueEditCommit);
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    TEST_ASSERT_EQUAL_STRING("", fixture.view().valueEdit->candidate.c_str());
    TEST_ASSERT_FALSE(fixture.view().bottomSlots[3].enabled);
    fixture.type("27.5");
    TEST_ASSERT_EQUAL_STRING("27.5",
                             fixture.view().valueEdit->candidate.c_str());
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);
    TEST_ASSERT_EQUAL_STRING("27.",
                             fixture.view().valueEdit->candidate.c_str());
    fixture.type("5");
    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() ==
                     FermentationUiPage::ProgramSummary);

    summary = *fixture.view().programSummary;
    TEST_ASSERT_EQUAL_DOUBLE(27.5, *summary.targetTemperatureCelsius);
    TEST_ASSERT_TRUE(summary.changed[static_cast<std::size_t>(
        FermentationUiStartField::TargetTemperature)]);
    TEST_ASSERT_TRUE(fixture.view().slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::ResetStartValues);

    // The start payload carries the override; the stored program is intact.
    const auto start = fixture.slot(2U);
    TEST_ASSERT_TRUE(start.action.has_value());
    const auto& candidate =
        std::get<FermentationUiStartProgramIntent>(*start.action).candidate;
    TEST_ASSERT_EQUAL_STRING(fixture.id.c_str(), candidate.programId.c_str());
    TEST_ASSERT_EQUAL_DOUBLE(27.5, *candidate.targetTemperatureCelsius);
    TEST_ASSERT_FALSE(candidate.fermentationDurationMinutes.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(
        *stored.program.fermentationStages.front().targetTemperatureCelsius,
        *fixture.catalog.programs.back()
             .program.fermentationStages.front()
             .targetTemperatureCelsius);
}

void test_start_value_edit_cancel_and_invalid_values_store_nothing() {
    StartValueFixture fixture;
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    fixture.type("999");
    // An out-of-range value (existing validator) cannot be committed.
    TEST_ASSERT_FALSE(fixture.view().valueEdit->commitValid);
    TEST_ASSERT_FALSE(fixture.view().bottomSlots[3].enabled);
    TEST_ASSERT_FALSE(fixture.slot(3U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::ValueEdit);
    // Cancel discards the edit.
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() ==
                     FermentationUiPage::ProgramSummary);
    TEST_ASSERT_EQUAL_DOUBLE(
        25.0, *fixture.view().programSummary->targetTemperatureCelsius);
    TEST_ASSERT_TRUE(fixture.view().slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::NavigateStatus);
    // A lone sign or an empty candidate is no value either.
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    fixture.type("-");
    TEST_ASSERT_FALSE(fixture.view().bottomSlots[3].enabled);
}

void test_whole_number_start_fields_have_no_decimal_or_sign_key() {
    StartValueFixture fixture;
    TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::ValueEdit);
    TEST_ASSERT_EQUAL_STRING("60", fixture.view().valueEdit->candidate.c_str());
    // `.` and `+/-` are disabled: no change, blocked feedback.
    TEST_ASSERT_FALSE(fixture.tap(3U, 0U).navigated);
    TEST_ASSERT_FALSE(fixture.tap(3U, 2U).navigated);
    TEST_ASSERT_EQUAL_STRING("60", fixture.view().valueEdit->candidate.c_str());
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    fixture.type("90");
    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
    TEST_ASSERT_EQUAL_UINT32(90U,
                             *fixture.view().programSummary->durationMinutes);
    // The input length is bounded.
    TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);
    for (int index = 0; index < 20; ++index) (void)fixture.tap(3U, 1U);
    TEST_ASSERT_TRUE(fixture.view().valueEdit->candidate.size() <= 8U);
}

void test_start_value_cycles_and_dependent_fields_stay_consistent() {
    StartValueFixture fixture;
    fixture.scrollTo(2U);  // rows: preheat, sensor, completion
    // Preheat toggles per tap.
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.view().programSummary->preheat);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_FALSE(fixture.view().programSummary->preheat);
    // Sensor (AirProductOptional, default Air): none -> Product -> none; only
    // candidate.sensorMode moves and Air (the default) is never an override.
    TEST_ASSERT_FALSE(
        fixture.view().programSummary->sensorModeOverride.has_value());
    TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.view().programSummary->sensorModeOverride ==
                     std::optional<RunSensorMode>{RunSensorMode::Product});
    TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);
    TEST_ASSERT_FALSE(
        fixture.view().programSummary->sensorModeOverride.has_value());
    TEST_ASSERT_FALSE(fixture.workspace.view(fixture.snapshot, &fixture.catalog)
                          .programSummary->changed[static_cast<std::size_t>(
                              FermentationUiStartField::SensorMode)]);

    // Completion cools: without a cooling target the values are invalid and
    // confirm stays disabled with the reason.
    TEST_ASSERT_TRUE(fixture.tap(2U, 0U).navigated);
    auto view = fixture.view();
    TEST_ASSERT_TRUE(view.programSummary->completionMode ==
                     CompletionMode::CoolThenFinish);
    TEST_ASSERT_EQUAL_UINT32(6U, view.programSummary->fieldCount);
    TEST_ASSERT_FALSE(view.programSummary->valuesValid);
    TEST_ASSERT_FALSE(view.bottomSlots[2].enabled);
    TEST_ASSERT_TRUE(view.blockedReason ==
                     fermentationTextKey("start-values-invalid"));
    TEST_ASSERT_FALSE(fixture.slot(2U).action.has_value());

    // The cooling target row appears; a valid value makes the start possible.
    fixture.scrollTo(5U);
    TEST_ASSERT_TRUE(fixture.view().programSummary->fields[5] ==
                     FermentationUiStartField::CoolingTarget);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::ValueEdit);
    TEST_ASSERT_FALSE(fixture.view().bottomSlots[3].enabled);  // empty
    fixture.type("8");
    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
    view = fixture.view();
    TEST_ASSERT_TRUE(view.programSummary->valuesValid);
    TEST_ASSERT_TRUE(view.bottomSlots[2].enabled);
    TEST_ASSERT_FALSE(view.blockedReason.has_value());
    const auto cooling = fixture.slot(2U);
    TEST_ASSERT_TRUE(cooling.action.has_value());
    const auto& candidate =
        std::get<FermentationUiStartProgramIntent>(*cooling.action).candidate;
    TEST_ASSERT_TRUE(
        candidate.completionMode ==
        std::optional<CompletionMode>{CompletionMode::CoolThenFinish});
    TEST_ASSERT_EQUAL_DOUBLE(8.0, *candidate.coolingTargetCelsius);

    // Next modes: hold-for-duration needs a hold duration; hold-until-stop
    // needs none; back to finish drops the cooling target override again.
    fixture.scrollTo(4U);  // completion is row 0 of the window
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    view = fixture.view();
    TEST_ASSERT_TRUE(view.programSummary->completionMode ==
                     CompletionMode::CoolAndHoldForDuration);
    TEST_ASSERT_EQUAL_UINT32(7U, view.programSummary->fieldCount);
    TEST_ASSERT_FALSE(view.programSummary->valuesValid);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    view = fixture.view();
    TEST_ASSERT_TRUE(view.programSummary->completionMode ==
                     CompletionMode::CoolAndHoldUntilManualStop);
    TEST_ASSERT_TRUE(view.programSummary->valuesValid);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    view = fixture.view();
    TEST_ASSERT_TRUE(view.programSummary->completionMode ==
                     CompletionMode::FinishWithoutCooling);
    TEST_ASSERT_EQUAL_UINT32(5U, view.programSummary->fieldCount);
    TEST_ASSERT_TRUE(view.programSummary->valuesValid);
    TEST_ASSERT_FALSE(view.programSummary->coolingTargetCelsius.has_value());
}

// B1: the sensor cycle offers only structurally allowed alternatives to the
// preference's canonical default (#21 rule); a mode equal to the default is
// no override (no mark, no reset); a structurally rejected candidate keeps
// confirm disabled. Sensor quality or availability is never evaluated here.
void test_sensor_cycle_follows_the_structural_start_matrix() {
    struct Case {
        SensorPreference preference;
        std::optional<RunSensorMode> alternative;  // none: row not cyclable
    };
    const Case cases[] = {
        {SensorPreference::ProductIfAvailableElseAir, RunSensorMode::Air},
        {SensorPreference::AirProductOptional, RunSensorMode::Product},
        {SensorPreference::ProductRequired, std::nullopt},
        {SensorPreference::AirOnly, std::nullopt},
    };
    for (const auto& item : cases) {
        StartValueFixture fixture;
        auto& program = fixture.catalog.programs.back().program;
        program.sensorPreference = item.preference;
        if (item.preference == SensorPreference::ProductRequired) {
            program.productSensorFailure.policy =
                ProductSensorFailurePolicy::WaitForUser;
            program.productSensorFailure.fallbackDelaySeconds.reset();
        } else if (item.preference == SensorPreference::AirOnly) {
            program.productSensorFailure.returnStrategy =
                ReturnStrategy::RemainOnAirUntilEnd;
            program.productSensorFailure.fallbackDelaySeconds.reset();
        }
        TEST_ASSERT_TRUE(
            fixture.workspace.selectProgram(fixture.id, fixture.catalog));
        fixture.scrollTo(2U);  // preheat, sensor, completion
        const auto before = fixture.workspace.renderRevision();
        const auto tapped = fixture.tap(1U, 0U);
        if (!item.alternative.has_value()) {
            // No alternative: the row is not interactive, nothing is staged.
            TEST_ASSERT_FALSE(tapped.navigated);
            TEST_ASSERT_FALSE(
                fixture.view().programSummary->sensorModeOverride.has_value());
            TEST_ASSERT_TRUE(fixture.view().slotActions[3] ==
                             FermentationUiWorkspaceSlotAction::NavigateStatus);
            continue;
        }
        (void)before;
        TEST_ASSERT_TRUE(tapped.navigated);
        auto view = fixture.view();
        TEST_ASSERT_TRUE(view.programSummary->sensorModeOverride ==
                         item.alternative);
        TEST_ASSERT_TRUE(view.programSummary->changed[static_cast<std::size_t>(
            FermentationUiStartField::SensorMode)]);
        TEST_ASSERT_TRUE(view.slotActions[3] ==
                         FermentationUiWorkspaceSlotAction::ResetStartValues);
        TEST_ASSERT_TRUE(view.bottomSlots[2].enabled);
        TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);
        view = fixture.view();
        TEST_ASSERT_FALSE(view.programSummary->sensorModeOverride.has_value());
        TEST_ASSERT_TRUE(view.slotActions[3] ==
                         FermentationUiWorkspaceSlotAction::NavigateStatus);
    }

    // An externally staged redundant override normalizes to no override.
    {
        StartValueFixture fixture;
        FermentationUiStartCandidate candidate;
        candidate.programId = fixture.id;
        candidate.sensorMode = RunSensorMode::Air;  // default of the program
        fixture.workspace.setStartCandidate(candidate);
        const auto view = fixture.view();
        TEST_ASSERT_FALSE(view.programSummary->sensorModeOverride.has_value());
        TEST_ASSERT_FALSE(view.programSummary->changed[static_cast<std::size_t>(
            FermentationUiStartField::SensorMode)]);
        TEST_ASSERT_TRUE(view.slotActions[3] ==
                         FermentationUiWorkspaceSlotAction::NavigateStatus);
        TEST_ASSERT_TRUE(view.bottomSlots[2].enabled);
    }
    // An externally staged structurally rejected candidate disables confirm.
    {
        StartValueFixture fixture;
        auto& program = fixture.catalog.programs.back().program;
        program.sensorPreference = SensorPreference::AirOnly;
        program.productSensorFailure.returnStrategy =
            ReturnStrategy::RemainOnAirUntilEnd;
        program.productSensorFailure.fallbackDelaySeconds.reset();
        TEST_ASSERT_TRUE(
            fixture.workspace.selectProgram(fixture.id, fixture.catalog));
        FermentationUiStartCandidate candidate;
        candidate.programId = fixture.id;
        candidate.sensorMode = RunSensorMode::Product;
        fixture.workspace.setStartCandidate(candidate);
        const auto view = fixture.view();
        TEST_ASSERT_FALSE(view.programSummary->valuesValid);
        TEST_ASSERT_FALSE(view.bottomSlots[2].enabled);
        TEST_ASSERT_FALSE(fixture.slot(2U).action.has_value());
    }
}

void test_reset_start_values_is_offered_only_for_a_changed_candidate() {
    StartValueFixture fixture;
    TEST_ASSERT_TRUE(fixture.view().slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::NavigateStatus);
    fixture.scrollTo(2U);
    TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);  // sensor override
    TEST_ASSERT_TRUE(fixture.view().slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::ResetStartValues);
    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
    const auto view = fixture.view();
    TEST_ASSERT_TRUE(view.slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::NavigateStatus);
    TEST_ASSERT_FALSE(view.programSummary->sensorModeOverride.has_value());
    // The start candidate keeps its identity, so the start stays possible.
    TEST_ASSERT_TRUE(view.bottomSlots[2].enabled);

    // An override equal to the stored value is no change.
    fixture.scrollTo(0U);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    fixture.type("25");
    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
    TEST_ASSERT_TRUE(fixture.view().slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::NavigateStatus);
}

void test_unstartable_program_start_fields_are_not_editable() {
    auto catalog = catalogWithPrograms(1U);
    catalog.programs.back().program.enabled = false;
    const auto snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    FermentationTouchWorkspace workspace;
    TEST_ASSERT_TRUE(
        workspace.selectProgram(catalog.programs.back().program.id, catalog));
    TEST_ASSERT_FALSE(
        workspace.view(snapshot, &catalog).programSummary->editable);
    TEST_ASSERT_FALSE(
        workspace.press(snapshot, cellAt(0U, 0U), &catalog).navigated);
    TEST_ASSERT_TRUE(workspace.page() == FermentationUiPage::ProgramSummary);
    // The owning reason stays; no start-values hint replaces it.
    TEST_ASSERT_TRUE(*workspace.view(snapshot, &catalog).blockedReason ==
                     fermentationTextKey("program-disabled"));
}

// S9: manual run values and cooling plans (real values only; start stays
// fail-closed until the technical limits have a released producer, O5).
struct ManualFixture {
    FermentationUiSnapshot snapshot;
    FermentationTouchWorkspace workspace;

    ManualFixture()
        : snapshot(snapshotFor(ProcessState::Standby,
                               FermentationHomeMode::Standby)) {}

    FermentationUiWorkspaceView view() const {
        return workspace.view(snapshot);
    }
    FermentationUiWorkspacePress tap(std::uint8_t row, std::uint8_t column) {
        return workspace.press(snapshot, cellAt(row, column));
    }
    FermentationUiWorkspacePress slot(std::uint8_t index) {
        return workspace.press(snapshot, bottom(index));
    }
    void type(const char* characters) {
        for (const char* c = characters; *c != '\0'; ++c) {
            std::uint8_t row = 3U;
            std::uint8_t column = 1U;
            if (*c >= '1' && *c <= '9') {
                row = static_cast<std::uint8_t>((*c - '1') / 3);
                column = static_cast<std::uint8_t>((*c - '1') % 3);
            } else if (*c == '.') {
                column = 0U;
            }
            TEST_ASSERT_TRUE(tap(row, column).navigated);
        }
    }
    // Opens the edit page of the row, replaces the value and commits it.
    void enter(std::uint8_t row, const char* value) {
        TEST_ASSERT_TRUE(tap(row, 0U).navigated);
        TEST_ASSERT_TRUE(workspace.page() == FermentationUiPage::ValueEdit);
        if (!view().valueEdit->candidate.empty())
            TEST_ASSERT_TRUE(slot(2U).navigated);  // clear
        type(value);
        TEST_ASSERT_TRUE(slot(3U).navigated);  // commit
    }
};

FermentationUiManualRunPlanValues stagedHolding() {
    FermentationUiManualRunPlanValues values;
    values.targetTemperatureCelsius = 30.0;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;
    return values;
}

ManualTimedRunValues stagedTimed() {
    ManualTimedRunValues values;
    values.targetTemperatureCelsius = 30.0;
    values.durationMinutes = 60U;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;
    return values;
}

void test_manual_pages_list_only_real_run_values() {
    ManualFixture holding;
    holding.workspace.setPage(FermentationUiPage::ManualHolding);
    auto view = holding.view();
    TEST_ASSERT_TRUE(view.programSummary.has_value());
    TEST_ASSERT_TRUE(view.programSummary->manual);
    TEST_ASSERT_EQUAL_UINT32(3U, view.programSummary->fieldCount);
    TEST_ASSERT_TRUE(view.programSummary->fields[0] ==
                     FermentationUiStartField::TargetTemperature);
    TEST_ASSERT_TRUE(view.programSummary->fields[1] ==
                     FermentationUiStartField::Preheat);
    TEST_ASSERT_TRUE(view.programSummary->fields[2] ==
                     FermentationUiStartField::SensorMode);
    // No number is invented: an unset target stays absent.
    TEST_ASSERT_FALSE(
        view.programSummary->targetTemperatureCelsius.has_value());
    TEST_ASSERT_FALSE(view.programSummary->pagerButtons);

    ManualFixture timed;
    timed.workspace.setPage(FermentationUiPage::ManualTimed);
    view = timed.view();
    TEST_ASSERT_EQUAL_UINT32(5U, view.programSummary->fieldCount);
    TEST_ASSERT_TRUE(view.programSummary->pagerButtons);
    // The technical limits are never a field of any manual page.
    for (const auto page :
         {FermentationUiPage::ManualHolding, FermentationUiPage::ManualTimed,
          FermentationUiPage::StopDialog, FermentationUiPage::Completion}) {
        ManualFixture fixture;
        fixture.workspace.setPage(page);
        const auto fields = fixture.view().programSummary;
        TEST_ASSERT_TRUE(fields.has_value());
        for (std::size_t index = 0U; index < fields->fieldCount; ++index) {
            const auto field = fields->fields[index];
            TEST_ASSERT_TRUE(
                field == FermentationUiStartField::TargetTemperature ||
                field == FermentationUiStartField::Duration ||
                field == FermentationUiStartField::Preheat ||
                field == FermentationUiStartField::SensorMode ||
                field == FermentationUiStartField::CompletionMode ||
                field == FermentationUiStartField::CoolingTarget ||
                field == FermentationUiStartField::HoldDuration);
        }
    }
}

void test_manual_start_is_disabled_with_a_reason_on_every_manual_page() {
    ManualFixture fixture;
    fixture.workspace.setManualHoldingValues(stagedHolding());
    fixture.workspace.setManualTimedValues(stagedTimed());
    auto cooling = stagedHolding();
    cooling.targetTemperatureCelsius = 8.0;
    fixture.workspace.setStopCoolingPlan(cooling);
    fixture.workspace.setCompletionCoolingPlan(cooling);
    struct Page {
        FermentationUiPage page;
        std::uint8_t slot;
        ProcessState state;
        FermentationHomeMode mode;
    };
    const Page pages[] = {
        {FermentationUiPage::ManualHolding, 2U, ProcessState::Standby,
         FermentationHomeMode::Standby},
        {FermentationUiPage::ManualTimed, 2U, ProcessState::Standby,
         FermentationHomeMode::Standby},
        {FermentationUiPage::StopDialog, 2U, ProcessState::Fermenting,
         FermentationHomeMode::ActiveRun},
        {FermentationUiPage::Completion, 3U, ProcessState::Completed,
         FermentationHomeMode::Completed},
    };
    for (const auto& item : pages) {
        fixture.snapshot = snapshotFor(item.state, item.mode);
        fixture.workspace.setPage(item.page);
        const auto view = fixture.view();
        // Staged values (the only way to fill the technical limits here) do
        // not open the start: no owner exists, O5.
        TEST_ASSERT_FALSE(view.bottomSlots[item.slot].enabled);
        TEST_ASSERT_TRUE(view.blockedReason ==
                         fermentationTextKey("manual-parameters-not-released"));
        TEST_ASSERT_FALSE(fixture.slot(item.slot).action.has_value());
    }
    fixture.snapshot =
        snapshotFor(ProcessState::Standby, FermentationHomeMode::Standby);
    fixture.workspace.setPage(FermentationUiPage::ManualHolding);
    TEST_ASSERT_FALSE(
        fixture.workspace
            .press(fixture.snapshot,
                   {device_platform::DeviceUiTargetKind::Confirm, 0U})
            .action.has_value());
}

void test_manual_holding_edits_stay_visible_real_values_without_a_start() {
    ManualFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::ManualHolding);
    fixture.enter(0U, "34.5");
    TEST_ASSERT_TRUE(fixture.workspace.page() ==
                     FermentationUiPage::ManualHolding);
    TEST_ASSERT_EQUAL_DOUBLE(
        34.5, *fixture.view().programSummary->targetTemperatureCelsius);
    TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);  // preheat
    TEST_ASSERT_TRUE(fixture.view().programSummary->preheat);
    TEST_ASSERT_TRUE(fixture.tap(2U, 0U).navigated);  // sensor
    TEST_ASSERT_TRUE(fixture.view().programSummary->sensorMode ==
                     RunSensorMode::Product);
    TEST_ASSERT_TRUE(fixture.tap(2U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.view().programSummary->sensorMode ==
                     RunSensorMode::Air);
    // The edits never open the start.
    TEST_ASSERT_FALSE(fixture.view().bottomSlots[2].enabled);
    TEST_ASSERT_FALSE(fixture.slot(2U).action.has_value());
}

void test_manual_values_use_the_canonical_limits_and_whole_numbers() {
    ManualFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::ManualTimed);
    // Target outside the fermentation range cannot be committed.
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    fixture.type("99");
    TEST_ASSERT_FALSE(fixture.view().valueEdit->commitValid);
    TEST_ASSERT_FALSE(fixture.slot(3U).navigated);
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    fixture.type("30");
    TEST_ASSERT_TRUE(fixture.view().valueEdit->commitValid);
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);  // cancel
    TEST_ASSERT_FALSE(
        fixture.view().programSummary->targetTemperatureCelsius.has_value());

    // Duration: whole minutes only, no decimal key, zero is outside the range.
    TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);
    TEST_ASSERT_FALSE(fixture.tap(3U, 0U).navigated);
    fixture.type("0");
    TEST_ASSERT_FALSE(fixture.view().valueEdit->commitValid);
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    fixture.type("90");
    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
    TEST_ASSERT_EQUAL_UINT32(90U,
                             *fixture.view().programSummary->durationMinutes);
}

void test_manual_timed_completion_cycle_keeps_only_the_used_real_values() {
    ManualFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::ManualTimed);
    while (fixture.view().pager.currentIndex < 3U)
        TEST_ASSERT_TRUE(fixture.tap(1U, 1U).navigated);
    TEST_ASSERT_TRUE(fixture.tap(1U, 0U).navigated);  // completion row
    auto view = fixture.view();
    TEST_ASSERT_TRUE(view.programSummary->completionMode ==
                     CompletionMode::CoolThenFinish);
    TEST_ASSERT_EQUAL_UINT32(6U, view.programSummary->fieldCount);
    while (fixture.view().pager.currentIndex < 5U)
        TEST_ASSERT_TRUE(fixture.tap(1U, 1U).navigated);
    fixture.enter(0U, "8");
    TEST_ASSERT_EQUAL_DOUBLE(
        8.0, *fixture.view().programSummary->coolingTargetCelsius);
    // Hold for a duration adds the hold time row, hold until stop drops it,
    // finish drops the cooling target again.
    while (fixture.view().pager.currentIndex > 4U)
        TEST_ASSERT_TRUE(fixture.tap(0U, 1U).navigated);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_EQUAL_UINT32(7U, fixture.view().programSummary->fieldCount);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_EQUAL_UINT32(6U, fixture.view().programSummary->fieldCount);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    view = fixture.view();
    TEST_ASSERT_EQUAL_UINT32(5U, view.programSummary->fieldCount);
    TEST_ASSERT_TRUE(view.programSummary->completionMode ==
                     CompletionMode::FinishWithoutCooling);
    TEST_ASSERT_FALSE(view.bottomSlots[2].enabled);
}

void test_cooling_plan_pages_edit_the_real_target_only() {
    for (const auto page :
         {FermentationUiPage::StopDialog, FermentationUiPage::Completion}) {
        ManualFixture fixture;
        if (page == FermentationUiPage::Completion) {
            fixture.snapshot = snapshotFor(ProcessState::Completed,
                                           FermentationHomeMode::Completed);
        }
        fixture.workspace.setPage(page);
        const std::uint8_t coolSlot =
            page == FermentationUiPage::StopDialog ? 2U : 3U;
        auto view = fixture.view();
        // One row, below the page's own content; no pager buttons.
        TEST_ASSERT_EQUAL_UINT32(1U, view.programSummary->fieldCount);
        TEST_ASSERT_EQUAL_UINT8(1U, view.programSummary->rowOffset);
        TEST_ASSERT_FALSE(view.programSummary->pagerButtons);
        TEST_ASSERT_TRUE(view.programSummary->fields[0] ==
                         FermentationUiStartField::CoolingTarget);
        // Row 0 is page content, not a field.
        TEST_ASSERT_FALSE(fixture.tap(0U, 0U).navigated);
        fixture.enter(1U, "6.5");
        view = fixture.view();
        TEST_ASSERT_EQUAL_DOUBLE(6.5,
                                 *view.programSummary->coolingTargetCelsius);
        // The cooling start stays closed (O5).
        TEST_ASSERT_FALSE(view.bottomSlots[coolSlot].enabled);
        TEST_ASSERT_FALSE(fixture.slot(coolSlot).action.has_value());
    }
}

// ---- S10: settings, keyboard, program editor --------------------------------

struct SettingsFixture {
    FermentationUiSnapshot snapshot;
    FermentationTouchWorkspace workspace;

    SettingsFixture()
        : snapshot(snapshotFor(ProcessState::Standby,
                               FermentationHomeMode::Standby)) {
        snapshot.revisions.expectedUserConfigurationRevision =
            UserConfigurationRevision{5U};
        snapshot.service.available = true;
    }
    FermentationUiWorkspaceView view() const {
        return workspace.view(snapshot);
    }
    FermentationUiWorkspacePress tap(std::uint8_t row, std::uint8_t column) {
        return workspace.press(snapshot, cellAt(row, column));
    }
    FermentationUiWorkspacePress slot(std::uint8_t index) {
        return workspace.press(snapshot, bottom(index));
    }
    void scrollTo(std::size_t index) {
        while (view().pager.currentIndex < index) {
            TEST_ASSERT_TRUE(slot(2U).navigated);
        }
        while (view().pager.currentIndex > index) {
            TEST_ASSERT_TRUE(slot(1U).navigated);
        }
    }
    // Types a string on the keyboard through its cells (letters of the
    // current mode, space, `-`, `.`).
    void typeText(const char* characters) {
        for (const char* c = characters; *c != '\0'; ++c) {
            bool found = false;
            for (std::uint8_t row = 0U; row < 4U && !found; ++row) {
                for (std::uint8_t column = 0U; column < 10U && !found;
                     ++column) {
                    const auto key = fermentationUiKeyboardKeyAt(
                        view().textEdit->mode, row, column);
                    if (key.kind == FermentationUiKeyboardKeyKind::Character &&
                        key.character == *c) {
                        TEST_ASSERT_TRUE(tap(row, column).navigated);
                        found = true;
                    }
                }
            }
            TEST_ASSERT_TRUE(found);
        }
    }
};

void test_standby_slot_three_opens_settings_and_service_lives_below_it() {
    SettingsFixture fixture;
    auto home = fixture.view();
    TEST_ASSERT_TRUE(home.slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::NavigateSettings);
    TEST_ASSERT_TRUE(home.bottomSlots[3].enabled);
    // No Service reason is shown on the home page any more.
    fixture.snapshot.service.available = false;
    fixture.snapshot.service.unavailableReason =
        fermentationTextKey("service-locked");
    home = fixture.view();
    TEST_ASSERT_FALSE(home.blockedReason.has_value());
    TEST_ASSERT_TRUE(home.bottomSlots[3].enabled);

    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Settings);

    // The canonical stacks (D14): Home -> Settings -> Service -> Pin.
    fixture.workspace.setPage(FermentationUiPage::Pin);
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Service);
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Settings);
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Home);
    fixture.workspace.setPage(FermentationUiPage::Service);
    TEST_ASSERT_TRUE(fixture.view().route.segments.size() >= 2U);
}

// O1: the order of the rows is part of the contract.
void test_settings_rows_are_in_the_decided_order_and_open_their_pages() {
    struct Row {
        std::size_t index;
        FermentationUiPage page;
    };
    const Row rows[] = {
        {0U, FermentationUiPage::HeaderLanguage},
        {1U, FermentationUiPage::HeaderClock},
        {2U, FermentationUiPage::TextEdit},
        {3U, FermentationUiPage::HeaderNetwork},
        {4U, FermentationUiPage::HeaderWebAccess},
        {5U, FermentationUiPage::Service},
    };
    TEST_ASSERT_EQUAL_UINT32(kFermentationUiSettingsRowCount,
                             sizeof(rows) / sizeof(rows[0]));
    for (const auto& row : rows) {
        SettingsFixture fixture;
        fixture.workspace.setPage(FermentationUiPage::Settings);
        TEST_ASSERT_EQUAL_UINT32(kFermentationUiSettingsRowCount,
                                 fixture.view().pager.itemCount);
        fixture.scrollTo(row.index);
        const auto opened = fixture.tap(0U, 0U);
        TEST_ASSERT_TRUE(opened.navigated);
        TEST_ASSERT_TRUE(fixture.workspace.page() == row.page);
        // Back returns to the settings page it was opened from.
        TEST_ASSERT_TRUE(fixture.slot(0U).navigated);
        TEST_ASSERT_TRUE(fixture.workspace.page() ==
                         FermentationUiPage::Settings);
    }
    // The three-row window: rows past the end are no target.
    SettingsFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::Settings);
    fixture.scrollTo(5U);
    TEST_ASSERT_FALSE(fixture.tap(1U, 0U).navigated);
    TEST_ASSERT_FALSE(fixture.tap(0U, 1U).navigated);
}

void test_settings_service_and_device_name_rows_state_when_disabled() {
    SettingsFixture fixture;
    fixture.snapshot.service.available = false;
    fixture.snapshot.service.unavailableReason =
        fermentationTextKey("service-locked");
    fixture.workspace.setPage(FermentationUiPage::Settings);
    fixture.scrollTo(3U);  // network, web access, service
    auto view = fixture.view();
    TEST_ASSERT_TRUE(view.settings.has_value());
    TEST_ASSERT_FALSE(view.settings->serviceAvailable);
    TEST_ASSERT_TRUE(view.settings->serviceReason ==
                     std::optional<device_platform::TextKey>{
                         fermentationTextKey("service-locked")});
    TEST_ASSERT_FALSE(fixture.tap(2U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Settings);
    fixture.snapshot.service.available = true;
    TEST_ASSERT_TRUE(fixture.tap(2U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Service);

    // The device name row is disabled while a run is active (display of the
    // O4 gate; the Application decides).
    SettingsFixture running;
    running.snapshot =
        snapshotFor(ProcessState::Fermenting, FermentationHomeMode::ActiveRun);
    running.snapshot.home.activeRunId = "e1-c1";
    running.workspace.setPage(FermentationUiPage::Settings);
    running.scrollTo(2U);
    TEST_ASSERT_FALSE(running.view().settings->deviceNameEditable);
    TEST_ASSERT_FALSE(running.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(running.workspace.page() == FermentationUiPage::Settings);
}

// The firmware texts are immutable static tables: every locale holds the same
// keys, none is empty, and the views point at static (not owned) storage.
void test_fermentation_text_packs_are_complete_static_tables() {
    const auto packs = makeFermentationUiTextPacks();
    TEST_ASSERT_EQUAL_UINT32(3U, packs.size());
    const auto& reference = packs.front().translations;
    TEST_ASSERT_EQUAL_UINT32(183U, reference.size());
    for (const auto& pack : packs) {
        TEST_ASSERT_EQUAL_UINT32(reference.size(), pack.translations.size());
        for (const auto& translation : pack.translations) {
            TEST_ASSERT_FALSE(translation.key.empty());
            TEST_ASSERT_FALSE(translation.value.empty());
            const auto match =
                std::find_if(reference.begin(), reference.end(),
                             [&translation](const auto& other) {
                                 return other.key == translation.key;
                             });
            TEST_ASSERT_TRUE(match != reference.end());
        }
    }
    // The views stay valid after the pack vector is copied and destroyed.
    std::string probe;
    {
        const auto copy = packs;
        probe = std::string{copy.back().translations.begin()->value};
    }
    TEST_ASSERT_FALSE(probe.empty());
}

void test_device_name_is_a_read_only_copy_that_invalidates_the_render_key() {
    SettingsFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::Settings);
    const auto first = fixture.workspace.renderRevision();
    fixture.workspace.adoptDeviceName("Keller");
    const auto adopted = fixture.workspace.renderRevision();
    TEST_ASSERT_TRUE(adopted != first);
    TEST_ASSERT_EQUAL_STRING("Keller",
                             fixture.view().settings->deviceName.c_str());
    // The same name again is no visible change.
    fixture.workspace.adoptDeviceName("Keller");
    TEST_ASSERT_EQUAL_UINT32(adopted, fixture.workspace.renderRevision());
}

void test_device_name_editor_commits_through_the_owner_command() {
    SettingsFixture fixture;
    fixture.workspace.adoptDeviceName("Keller");
    fixture.workspace.setPage(FermentationUiPage::Settings);
    fixture.scrollTo(2U);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::TextEdit);
    auto view = fixture.view();
    TEST_ASSERT_TRUE(view.textEdit->target ==
                     FermentationUiTextTarget::DeviceName);
    // Prefilled with the owner's name; valid as it stands.
    TEST_ASSERT_EQUAL_STRING("Keller", view.textEdit->candidate.c_str());
    TEST_ASSERT_TRUE(view.textEdit->commitValid);
    TEST_ASSERT_TRUE(view.slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::TextEditCancel);
    TEST_ASSERT_TRUE(view.slotActions[1] ==
                     FermentationUiWorkspaceSlotAction::TextEditMode);
    TEST_ASSERT_TRUE(view.slotActions[2] ==
                     FermentationUiWorkspaceSlotAction::TextEditBackspace);
    TEST_ASSERT_TRUE(view.slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::TextEditCommit);

    // Clear cell (columns 0-1 of row 3), then type with the letter mode,
    // upper case mode, a space and a hyphen.
    TEST_ASSERT_TRUE(fixture.tap(3U, 1U).navigated);
    TEST_ASSERT_TRUE(fixture.view().textEdit->candidate.empty());
    TEST_ASSERT_FALSE(fixture.view().textEdit->commitValid);
    TEST_ASSERT_FALSE(fixture.slot(3U).navigated);
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);  // upper case
    TEST_ASSERT_TRUE(fixture.view().textEdit->mode == TextEditMode::Uppercase);
    fixture.typeText("G");
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);  // digits
    TEST_ASSERT_TRUE(fixture.view().textEdit->mode == TextEditMode::Digits);
    fixture.typeText("2 -");
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);  // symbols
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);  // lower case again
    TEST_ASSERT_TRUE(fixture.view().textEdit->mode == TextEditMode::Lowercase);
    fixture.typeText("x");
    TEST_ASSERT_EQUAL_STRING("G2 -x",
                             fixture.view().textEdit->candidate.c_str());
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);  // backspace
    TEST_ASSERT_EQUAL_STRING("G2 -",
                             fixture.view().textEdit->candidate.c_str());
    // A trailing hyphen is allowed by the name rule, the old text is gone.
    fixture.typeText("b");

    const auto commit = fixture.slot(3U);
    TEST_ASSERT_TRUE(commit.setDeviceName.has_value());
    TEST_ASSERT_EQUAL_STRING("G2 -b", commit.setDeviceName->deviceName.c_str());
    TEST_ASSERT_TRUE(
        commit.setDeviceName->expectedUserConfigurationRevision ==
        fixture.snapshot.revisions.expectedUserConfigurationRevision);
    TEST_ASSERT_FALSE(commit.action.has_value());
    TEST_ASSERT_FALSE(commit.programEdit.has_value());
    // The page returns to the settings; the owner decides the outcome.
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Settings);
    // The workspace keeps showing the owner's name until the owner changes it.
    TEST_ASSERT_EQUAL_STRING("Keller",
                             fixture.view().settings->deviceName.c_str());

    // A refused change is shown on the settings page until the next attempt.
    fixture.workspace.noteDeviceNameOutcome(false);
    TEST_ASSERT_TRUE(fixture.view().blockedReason ==
                     std::optional<device_platform::TextKey>{
                         fermentationTextKey("device-name-change-failed")});
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_FALSE(fixture.view().blockedReason.has_value());
    // Cancel leaves without a command.
    const auto cancel = fixture.slot(0U);
    TEST_ASSERT_TRUE(cancel.navigated);
    TEST_ASSERT_FALSE(cancel.setDeviceName.has_value());
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Settings);
}

void test_keyboard_follows_the_owning_text_rules_and_byte_limit() {
    SettingsFixture fixture;
    fixture.workspace.adoptDeviceName("Name");
    fixture.workspace.setPage(FermentationUiPage::Settings);
    fixture.scrollTo(2U);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    // Leading space, trailing space, too many characters: no commit.
    TEST_ASSERT_TRUE(fixture.tap(3U, 0U).navigated);  // clear
    fixture.typeText(" a");
    TEST_ASSERT_FALSE(fixture.view().textEdit->commitValid);
    TEST_ASSERT_TRUE(fixture.tap(3U, 0U).navigated);
    fixture.typeText("a ");
    TEST_ASSERT_FALSE(fixture.view().textEdit->commitValid);
    TEST_ASSERT_TRUE(fixture.tap(3U, 0U).navigated);
    for (int index = 0; index < 48; ++index) fixture.typeText("a");
    TEST_ASSERT_TRUE(fixture.view().textEdit->commitValid);
    fixture.typeText("a");  // 49 characters: still typeable, not valid
    TEST_ASSERT_FALSE(fixture.view().textEdit->commitValid);
    TEST_ASSERT_FALSE(fixture.slot(3U).navigated);
    // The byte limit stops further input (the cells report blocked).
    while (!fixture.view().textEdit->full) fixture.typeText("a");
    TEST_ASSERT_EQUAL_UINT32(96U, fixture.view().textEdit->candidate.size());
    TEST_ASSERT_FALSE(fixture.tap(0U, 0U).navigated);
    // Backspace and Clear stay possible.
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    TEST_ASSERT_FALSE(fixture.view().textEdit->full);
}

void test_keyboard_backspace_keeps_a_multibyte_name_valid_utf8() {
    SettingsFixture fixture;
    fixture.workspace.adoptDeviceName(
        "K\xC3\xBC"
        "che\xE2\x82\xAC");
    fixture.workspace.setPage(FermentationUiPage::Settings);
    fixture.scrollTo(2U);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.view().textEdit->commitValid);
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);  // removes the euro sign
    TEST_ASSERT_EQUAL_STRING(
        "K\xC3\xBC"
        "che",
        fixture.view().textEdit->candidate.c_str());
    for (int step = 0; step < 3; ++step)
        TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    TEST_ASSERT_EQUAL_STRING("K\xC3\xBC",
                             fixture.view().textEdit->candidate.c_str());
    TEST_ASSERT_TRUE(fixture.view().textEdit->commitValid);
    TEST_ASSERT_TRUE(fixture.slot(2U).navigated);
    TEST_ASSERT_EQUAL_STRING("K", fixture.view().textEdit->candidate.c_str());
}

// ---- program editor ---------------------------------------------------------

struct EditorFixture {
    FermentationUiSnapshot snapshot;
    ProgramCatalog catalog;
    FermentationTouchWorkspace workspace;
    std::string id;

    EditorFixture()
        : snapshot(snapshotFor(ProcessState::Standby,
                               FermentationHomeMode::Standby)),
          catalog(catalogWithPrograms(1U)) {
        snapshot.revisions.expectedUserConfigurationRevision =
            UserConfigurationRevision{5U};
        auto& program = catalog.programs.back().program;
        program.name = "Brot";
        program.notes = "alt";
        program.preheat = false;
        program.maximumProductWaitMinutes.reset();
        program.sensorPreference = SensorPreference::AirProductOptional;
        program.productSensorFailure.policy =
            ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
        program.productSensorFailure.fallbackDelaySeconds = 60U;
        program.productSensorFailure.returnStrategy =
            ReturnStrategy::ManualReturnToProduct;
        program.completion.mode = CompletionMode::FinishWithoutCooling;
        program.completion.coolingTargetCelsius.reset();
        program.completion.holdDurationMinutes.reset();
        id = program.id;
        TEST_ASSERT_TRUE(workspace.selectProgram(id, catalog));
        workspace.setProgramEditOperation(
            FermentationUiProgramEditOperation::Edit);
        workspace.setPage(FermentationUiPage::ProgramEdit);
    }
    FermentationUiWorkspaceView view() const {
        return workspace.view(snapshot, &catalog);
    }
    FermentationUiWorkspacePress tap(std::uint8_t row, std::uint8_t column) {
        return workspace.press(snapshot, cellAt(row, column), &catalog);
    }
    FermentationUiWorkspacePress slot(std::uint8_t index) {
        return workspace.press(snapshot, bottom(index), &catalog);
    }
    void scrollTo(std::size_t index) {
        while (view().pager.currentIndex < index)
            TEST_ASSERT_TRUE(tap(1U, 1U).navigated);
        while (view().pager.currentIndex > index)
            TEST_ASSERT_TRUE(tap(0U, 1U).navigated);
    }
    // Index of a field in the current row list.
    std::size_t indexOf(FermentationUiProgramField field) const {
        const auto edit = view().programEdit;
        for (std::size_t index = 0U; index < edit->rowCount; ++index) {
            if (edit->rows[index].field == field) return index;
        }
        return edit->rowCount;
    }
    // Taps the row of the field (scrolled into the window).
    FermentationUiWorkspacePress tapField(FermentationUiProgramField field) {
        const auto index = indexOf(field);
        TEST_ASSERT_TRUE(index < view().programEdit->rowCount);
        scrollTo(index);
        return tap(0U, 0U);
    }
    void typeDigits(const char* digits) {
        for (const char* c = digits; *c != '\0'; ++c) {
            std::uint8_t row = 3U;
            std::uint8_t column = 1U;
            if (*c >= '1' && *c <= '9') {
                row = static_cast<std::uint8_t>((*c - '1') / 3);
                column = static_cast<std::uint8_t>((*c - '1') % 3);
            } else if (*c == '.') {
                column = 0U;
            }
            TEST_ASSERT_TRUE(tap(row, column).navigated);
        }
    }
    void setNumeric(FermentationUiProgramField field, const char* digits) {
        TEST_ASSERT_TRUE(tapField(field).navigated);
        TEST_ASSERT_TRUE(workspace.page() == FermentationUiPage::ValueEdit);
        while (!view().valueEdit->candidate.empty())
            TEST_ASSERT_TRUE(slot(1U).navigated);  // backspace
        typeDigits(digits);
        TEST_ASSERT_TRUE(slot(3U).navigated);
        TEST_ASSERT_TRUE(workspace.page() == FermentationUiPage::ProgramEdit);
    }
};

void test_program_editor_lists_the_local_fields_in_a_fixed_order() {
    EditorFixture fixture;
    auto edit = *fixture.view().programEdit;
    using Field = FermentationUiProgramField;
    // Preheat off, fallback policy, finish without cooling: no wait row, the
    // delay row is present, no cooling or hold rows.
    const Field expected[] = {
        Field::Name,           Field::Notes,         Field::TargetTemperature,
        Field::Duration,       Field::Preheat,       Field::SensorPreference,
        Field::FailurePolicy,  Field::FallbackDelay, Field::ReturnStrategy,
        Field::MaxTargetReach, Field::CompletionMode};
    TEST_ASSERT_EQUAL_UINT32(sizeof(expected) / sizeof(expected[0]),
                             edit.rowCount);
    for (std::size_t index = 0U; index < edit.rowCount; ++index)
        TEST_ASSERT_TRUE(edit.rows[index].field == expected[index]);
    // The technical qualification values are never a row.
    TEST_ASSERT_TRUE(fixture.view().pager.itemCount == edit.rowCount);
    // Without a change nothing is marked, and there is nothing to save yet.
    for (std::size_t index = 0U; index < edit.rowCount; ++index)
        TEST_ASSERT_FALSE(edit.rows[index].changed);
    TEST_ASSERT_FALSE(edit.valid);
    TEST_ASSERT_FALSE(fixture.view().bottomSlots[3].enabled);
}

void test_program_editor_cycles_drop_values_the_setting_makes_unexpected() {
    EditorFixture fixture;
    using Field = FermentationUiProgramField;
    // Preheat on adds the product wait row; off removes it and its value.
    TEST_ASSERT_TRUE(fixture.tapField(Field::Preheat).navigated);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::MaxProductWait) <
                     fixture.view().programEdit->rowCount);
    fixture.setNumeric(Field::MaxProductWait, "30");
    TEST_ASSERT_TRUE(fixture.tapField(Field::Preheat).navigated);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::MaxProductWait) ==
                     fixture.view().programEdit->rowCount);
    TEST_ASSERT_TRUE(fixture.view().programEdit->valid);

    // Sensor preference cycles: Product-if-available -> ... The sequence from
    // AirProductOptional goes to ProductRequired, then AirOnly, then back.
    TEST_ASSERT_TRUE(fixture.tapField(Field::SensorPreference).navigated);
    // ProductRequired with the fallback-to-air policy is an incompatible
    // combination: the existing validator makes the save unavailable.
    TEST_ASSERT_FALSE(fixture.view().programEdit->valid);
    TEST_ASSERT_FALSE(fixture.view().bottomSlots[3].enabled);
    // Changing the failure policy fixes it (and drops the fallback delay).
    TEST_ASSERT_TRUE(fixture.tapField(Field::FailurePolicy).navigated);
    TEST_ASSERT_TRUE(fixture.view().programEdit->valid);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::FallbackDelay) ==
                     fixture.view().programEdit->rowCount);
    TEST_ASSERT_TRUE(fixture.view().bottomSlots[3].enabled);
    // AirOnly has one valid combination: it is set and its rows are hidden.
    TEST_ASSERT_TRUE(fixture.tapField(Field::SensorPreference).navigated);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::FailurePolicy) ==
                     fixture.view().programEdit->rowCount);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::ReturnStrategy) ==
                     fixture.view().programEdit->rowCount);
    TEST_ASSERT_TRUE(fixture.view().programEdit->valid);

    // Completion: cool then finish adds the cooling target; hold for a
    // duration adds the hold time; finish drops both.
    TEST_ASSERT_TRUE(fixture.tapField(Field::CompletionMode).navigated);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::CoolingTarget) <
                     fixture.view().programEdit->rowCount);
    fixture.setNumeric(Field::CoolingTarget, "8");
    TEST_ASSERT_TRUE(fixture.tapField(Field::CompletionMode).navigated);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::HoldDuration) <
                     fixture.view().programEdit->rowCount);
    fixture.setNumeric(Field::HoldDuration, "90");
    TEST_ASSERT_TRUE(fixture.tapField(Field::CompletionMode).navigated);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::HoldDuration) ==
                     fixture.view().programEdit->rowCount);
    TEST_ASSERT_TRUE(fixture.tapField(Field::CompletionMode).navigated);
    TEST_ASSERT_TRUE(fixture.indexOf(Field::CoolingTarget) ==
                     fixture.view().programEdit->rowCount);
    TEST_ASSERT_TRUE(fixture.view().programEdit->valid);
}

void test_program_editor_numeric_fields_use_the_program_validator() {
    EditorFixture fixture;
    using Field = FermentationUiProgramField;
    // Out of the validator's range: no commit; whole numbers have no decimal.
    TEST_ASSERT_TRUE(fixture.tapField(Field::TargetTemperature).navigated);
    while (!fixture.view().valueEdit->candidate.empty())
        TEST_ASSERT_TRUE(fixture.slot(1U).navigated);
    fixture.typeDigits("999");
    TEST_ASSERT_FALSE(fixture.view().valueEdit->commitValid);
    TEST_ASSERT_TRUE(fixture.view().valueEdit->unit ==
                     FermentationUiValueUnit::Celsius);
    TEST_ASSERT_FALSE(fixture.view().valueEdit->wholeNumber);
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);  // cancel
    TEST_ASSERT_FALSE(
        fixture.view()
            .programEdit->rows[fixture.indexOf(Field::TargetTemperature)]
            .changed);

    TEST_ASSERT_TRUE(fixture.tapField(Field::FallbackDelay).navigated);
    TEST_ASSERT_TRUE(fixture.view().valueEdit->unit ==
                     FermentationUiValueUnit::Seconds);
    TEST_ASSERT_TRUE(fixture.view().valueEdit->wholeNumber);
    TEST_ASSERT_FALSE(fixture.tap(3U, 0U).navigated);  // no decimal key
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);

    fixture.setNumeric(Field::TargetTemperature, "27.5");
    fixture.setNumeric(Field::Duration, "90");
    fixture.setNumeric(Field::MaxTargetReach, "120");
    const auto edit = *fixture.view().programEdit;
    TEST_ASSERT_TRUE(edit.valid);
    TEST_ASSERT_TRUE(
        edit.rows[fixture.indexOf(Field::TargetTemperature)].changed);
    TEST_ASSERT_EQUAL_STRING(
        "27.5 C",
        edit.rows[fixture.indexOf(Field::TargetTemperature)].text.c_str());
    TEST_ASSERT_EQUAL_STRING(
        "90 min", edit.rows[fixture.indexOf(Field::Duration)].text.c_str());
    TEST_ASSERT_FALSE(edit.rows[fixture.indexOf(Field::Preheat)].changed);
    // The stored program is untouched.
    TEST_ASSERT_EQUAL_DOUBLE(25.0, *fixture.catalog.programs.back()
                                        .program.fermentationStages.front()
                                        .targetTemperatureCelsius);
}

void test_program_editor_name_and_notes_use_the_keyboard_and_save_the_candidate() {
    EditorFixture fixture;
    using Field = FermentationUiProgramField;
    TEST_ASSERT_TRUE(fixture.tapField(Field::Name).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::TextEdit);
    auto view = fixture.view();
    TEST_ASSERT_TRUE(view.textEdit->target ==
                     FermentationUiTextTarget::ProgramName);
    TEST_ASSERT_EQUAL_STRING("Brot", view.textEdit->candidate.c_str());
    // Clear, then "Roggen" through the cells (upper then lower case).
    TEST_ASSERT_TRUE(fixture.tap(3U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);
    const auto press = [&fixture](char c) {
        for (std::uint8_t row = 0U; row < 3U; ++row)
            for (std::uint8_t column = 0U; column < 10U; ++column) {
                const auto key = fermentationUiKeyboardKeyAt(
                    fixture.view().textEdit->mode, row, column);
                if (key.kind == FermentationUiKeyboardKeyKind::Character &&
                    key.character == c) {
                    TEST_ASSERT_TRUE(fixture.tap(row, column).navigated);
                    return;
                }
            }
        TEST_ASSERT_TRUE(false);
    };
    press('R');
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);
    TEST_ASSERT_TRUE(fixture.slot(1U).navigated);
    for (const char c : std::string("oggen")) press(c);
    TEST_ASSERT_EQUAL_STRING("Roggen",
                             fixture.view().textEdit->candidate.c_str());
    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() ==
                     FermentationUiPage::ProgramEdit);
    auto edit = *fixture.view().programEdit;
    TEST_ASSERT_EQUAL_STRING("Roggen", edit.rows[0].text.c_str());
    TEST_ASSERT_TRUE(edit.rows[0].changed);
    TEST_ASSERT_TRUE(edit.valid);

    // The note: empty is a valid note.
    TEST_ASSERT_TRUE(fixture.tapField(Field::Notes).navigated);
    TEST_ASSERT_TRUE(fixture.view().textEdit->target ==
                     FermentationUiTextTarget::ProgramNotes);
    TEST_ASSERT_TRUE(fixture.tap(3U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.view().textEdit->commitValid);
    TEST_ASSERT_TRUE(fixture.slot(3U).navigated);

    // The edits are dirty: leaving needs the discard confirmation.
    const auto blockedBack = fixture.workspace.press(
        fixture.snapshot, {device_platform::DeviceUiTargetKind::Back, 0U},
        &fixture.catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Blocked),
        static_cast<int>(blockedBack.interaction.outcome));

    // Save: the request carries the candidate with both changes.
    const auto save = fixture.slot(3U);
    TEST_ASSERT_TRUE(save.programEdit.has_value());
    TEST_ASSERT_TRUE(save.programEdit->operation ==
                     FermentationUiProgramEditOperation::Edit);
    TEST_ASSERT_EQUAL_STRING(fixture.id.c_str(),
                             save.programEdit->programId.c_str());
    TEST_ASSERT_TRUE(save.programEdit->candidate.has_value());
    TEST_ASSERT_EQUAL_STRING("Roggen",
                             save.programEdit->candidate->program.name.c_str());
    TEST_ASSERT_EQUAL_STRING(
        "", save.programEdit->candidate->program.notes.c_str());
    TEST_ASSERT_TRUE(save.programEdit->confirmed);
    TEST_ASSERT_FALSE(save.programEdit->name.has_value());
    // The stored program is untouched until the owner applies the request.
    TEST_ASSERT_EQUAL_STRING(
        "Brot", fixture.catalog.programs.back().program.name.c_str());
}

// B2: the shared keyboard page shows its real caller in the route and returns
// to it on cancel and on commit (the program candidate survives).
std::vector<std::string> routeValues(const FermentationUiWorkspaceView& view) {
    std::vector<std::string> values;
    for (const auto& segment : view.route.segments)
        values.push_back(segment.value);
    return values;
}

void test_text_edit_route_follows_the_device_name_caller() {
    SettingsFixture fixture;
    fixture.workspace.adoptDeviceName("Keller");
    fixture.workspace.setPage(FermentationUiPage::Settings);
    fixture.scrollTo(2U);
    TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::TextEdit);
    const auto route = routeValues(fixture.view());
    TEST_ASSERT_EQUAL_UINT32(3U, route.size());
    TEST_ASSERT_EQUAL_STRING("home", route[0].c_str());
    TEST_ASSERT_EQUAL_STRING("settings", route[1].c_str());
    TEST_ASSERT_EQUAL_STRING("edit", route[2].c_str());
    // Cancel returns to Settings.
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);
    TEST_ASSERT_TRUE(fixture.workspace.page() == FermentationUiPage::Settings);
}

void test_text_edit_route_follows_the_program_editor_caller() {
    for (const auto field : {FermentationUiProgramField::Name,
                             FermentationUiProgramField::Notes}) {
        EditorFixture fixture;
        TEST_ASSERT_TRUE(fixture.tapField(field).navigated);
        TEST_ASSERT_TRUE(fixture.workspace.page() ==
                         FermentationUiPage::TextEdit);
        const std::vector<std::string> expectedValues{"home", "programs",
                                                      "details"};
        auto route = routeValues(fixture.view());
        TEST_ASSERT_EQUAL_UINT32(expectedValues.size() + 1U, route.size());
        for (std::size_t index = 0U; index < expectedValues.size(); ++index)
            TEST_ASSERT_EQUAL_STRING(expectedValues[index].c_str(),
                                     route[index].c_str());
        TEST_ASSERT_EQUAL_STRING("edit", route.back().c_str());
        TEST_ASSERT_TRUE(route[1] != "settings");

        // Cancel: back in the editor, candidate untouched.
        TEST_ASSERT_TRUE(fixture.slot(0U).navigated);
        TEST_ASSERT_TRUE(fixture.workspace.page() ==
                         FermentationUiPage::ProgramEdit);
        // Commit: back in the editor with the typed change in the candidate.
        TEST_ASSERT_TRUE(fixture.tapField(field).navigated);
        TEST_ASSERT_TRUE(fixture.slot(1U).navigated);
        TEST_ASSERT_TRUE(fixture.slot(1U).navigated);     // digits
        TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);  // '1'
        TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
        TEST_ASSERT_TRUE(fixture.workspace.page() ==
                         FermentationUiPage::ProgramEdit);
        const auto edit = *fixture.view().programEdit;
        TEST_ASSERT_TRUE(edit.rows[fixture.indexOf(field)].changed);
    }
}

void test_program_editor_copy_and_new_take_a_request_name_only() {
    for (const auto operation : {FermentationUiProgramEditOperation::Copy,
                                 FermentationUiProgramEditOperation::New}) {
        EditorFixture fixture;
        fixture.workspace.setProgramEditOperation(operation);
        auto edit = *fixture.view().programEdit;
        TEST_ASSERT_EQUAL_UINT32(1U, edit.rowCount);
        TEST_ASSERT_TRUE(edit.rows[0].field ==
                         FermentationUiProgramField::Name);
        TEST_ASSERT_TRUE(fixture.view().bottomSlots[3].enabled);
        TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);
        TEST_ASSERT_TRUE(fixture.workspace.page() ==
                         FermentationUiPage::TextEdit);
        // A new name starts empty; the keyboard types it.
        TEST_ASSERT_TRUE(fixture.view().textEdit->candidate.empty());
        TEST_ASSERT_TRUE(fixture.slot(1U).navigated);
        TEST_ASSERT_TRUE(fixture.slot(1U).navigated);     // digits
        TEST_ASSERT_TRUE(fixture.tap(0U, 0U).navigated);  // '1'
        TEST_ASSERT_TRUE(fixture.slot(3U).navigated);
        edit = *fixture.view().programEdit;
        TEST_ASSERT_EQUAL_STRING("1", edit.rows[0].text.c_str());
        const auto save = fixture.slot(3U);
        TEST_ASSERT_TRUE(save.programEdit.has_value());
        TEST_ASSERT_TRUE(save.programEdit->operation == operation);
        TEST_ASSERT_TRUE(save.programEdit->name ==
                         std::optional<std::string>{"1"});
        TEST_ASSERT_FALSE(save.programEdit->candidate.has_value());
    }
}

void test_program_editor_edits_are_dirty_and_never_touch_the_stored_program() {
    EditorFixture fixture;
    using Field = FermentationUiProgramField;
    TEST_ASSERT_TRUE(fixture.view().route.exitRequirement ==
                     device_platform::PageExitRequirement::None);
    TEST_ASSERT_TRUE(fixture.view().slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::NavigateBack);
    fixture.setNumeric(Field::Duration, "75");
    // Back from the value page keeps the editor dirty (a sub-step, no discard).
    TEST_ASSERT_TRUE(fixture.view().route.exitRequirement ==
                     device_platform::PageExitRequirement::ConfirmDiscard);
    TEST_ASSERT_EQUAL_UINT32(60U, *fixture.catalog.programs.back()
                                       .program.fermentationStages.front()
                                       .durationMinutes);
    TEST_ASSERT_EQUAL_STRING(
        "75 min", fixture.view()
                      .programEdit->rows[fixture.indexOf(Field::Duration)]
                      .text.c_str());
}

// B1: the editor is clean only after the owner accepted the save request. The
// request itself leaves it dirty; a refused outcome keeps candidate and the
// discard protection, an accepted one releases both.
void test_program_save_marks_the_editor_clean_only_after_the_owner_accepts() {
    EditorFixture fixture;
    using Field = FermentationUiProgramField;
    fixture.setNumeric(Field::Duration, "75");
    const auto save = fixture.slot(3U);
    TEST_ASSERT_TRUE(save.programEdit.has_value());
    // The request alone does not clean the editor.
    TEST_ASSERT_TRUE(fixture.view().route.exitRequirement ==
                     device_platform::PageExitRequirement::ConfirmDiscard);

    // Refused by the owner (stale revision, persistence error, ...).
    fixture.workspace.noteProgramEditOutcome(false);
    auto view = fixture.view();
    TEST_ASSERT_TRUE(view.route.exitRequirement ==
                     device_platform::PageExitRequirement::ConfirmDiscard);
    TEST_ASSERT_EQUAL_STRING(
        "75 min",
        view.programEdit->rows[fixture.indexOf(Field::Duration)].text.c_str());
    TEST_ASSERT_TRUE(
        view.programEdit->rows[fixture.indexOf(Field::Duration)].changed);
    TEST_ASSERT_TRUE(view.slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::DiscardProgramEdit);
    TEST_ASSERT_TRUE(view.bottomSlots[3].enabled);  // retry possible
    const auto blocked = fixture.workspace.press(
        fixture.snapshot, {device_platform::DeviceUiTargetKind::Back, 0U},
        &fixture.catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Blocked),
        static_cast<int>(blocked.interaction.outcome));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiFeedbackIntent::ConfirmationRequired),
        static_cast<int>(blocked.interaction.feedback));
    TEST_ASSERT_TRUE(fixture.workspace.page() ==
                     FermentationUiPage::ProgramEdit);

    // Accepted by the owner: clean, the candidate is released (the owner's
    // catalog is the truth again), leaving works.
    fixture.workspace.noteProgramEditOutcome(true);
    view = fixture.view();
    TEST_ASSERT_TRUE(view.route.exitRequirement ==
                     device_platform::PageExitRequirement::None);
    TEST_ASSERT_TRUE(view.slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::NavigateBack);
    TEST_ASSERT_FALSE(view.bottomSlots[3].enabled);
    TEST_ASSERT_TRUE(fixture.slot(0U).navigated);
}

// A dirty editor cannot be left through the navigation exits; the explicit
// discard slot is the confirmation. It drops the candidate and the dirty flag,
// so reopening the editor shows the stored program again.
void test_program_editor_discard_is_an_explicit_slot_and_drops_the_candidate() {
    EditorFixture fixture;
    using Field = FermentationUiProgramField;
    fixture.setNumeric(Field::Duration, "75");
    const auto blockedBack = fixture.workspace.press(
        fixture.snapshot, {device_platform::DeviceUiTargetKind::Back, 0U},
        &fixture.catalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Blocked),
        static_cast<int>(blockedBack.interaction.outcome));
    TEST_ASSERT_TRUE(fixture.workspace.page() ==
                     FermentationUiPage::ProgramEdit);
    TEST_ASSERT_TRUE(fixture.view().slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::DiscardProgramEdit);
    TEST_ASSERT_TRUE(fixture.view().bottomSlots[0].enabled);

    const auto discard = fixture.slot(0U);
    TEST_ASSERT_TRUE(discard.navigated);
    TEST_ASSERT_FALSE(discard.programEdit.has_value());
    TEST_ASSERT_TRUE(fixture.workspace.page() ==
                     FermentationUiPage::ProgramSummary);
    fixture.workspace.setPage(FermentationUiPage::ProgramEdit);
    const auto edit = *fixture.view().programEdit;
    TEST_ASSERT_EQUAL_STRING(
        "60 min", edit.rows[fixture.indexOf(Field::Duration)].text.c_str());
    for (std::size_t index = 0U; index < edit.rowCount; ++index)
        TEST_ASSERT_FALSE(edit.rows[index].changed);
    TEST_ASSERT_FALSE(
        fixture.view().bottomSlots[3].enabled);  // nothing to save
    TEST_ASSERT_TRUE(fixture.view().slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::NavigateBack);
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
        test_recovery_time_correction_is_never_offered_even_with_a_staged_value);
    RUN_TEST(test_start_value_keypad_edit_sets_only_the_next_run_candidate);
    RUN_TEST(test_start_value_edit_cancel_and_invalid_values_store_nothing);
    RUN_TEST(test_whole_number_start_fields_have_no_decimal_or_sign_key);
    RUN_TEST(test_start_value_cycles_and_dependent_fields_stay_consistent);
    RUN_TEST(test_sensor_cycle_follows_the_structural_start_matrix);
    RUN_TEST(test_reset_start_values_is_offered_only_for_a_changed_candidate);
    RUN_TEST(test_unstartable_program_start_fields_are_not_editable);
    RUN_TEST(test_manual_pages_list_only_real_run_values);
    RUN_TEST(test_manual_start_is_disabled_with_a_reason_on_every_manual_page);
    RUN_TEST(
        test_manual_holding_edits_stay_visible_real_values_without_a_start);
    RUN_TEST(test_manual_values_use_the_canonical_limits_and_whole_numbers);
    RUN_TEST(
        test_manual_timed_completion_cycle_keeps_only_the_used_real_values);
    RUN_TEST(test_cooling_plan_pages_edit_the_real_target_only);
    RUN_TEST(test_standby_slot_three_opens_settings_and_service_lives_below_it);
    RUN_TEST(test_settings_rows_are_in_the_decided_order_and_open_their_pages);
    RUN_TEST(test_settings_service_and_device_name_rows_state_when_disabled);
    RUN_TEST(
        test_device_name_is_a_read_only_copy_that_invalidates_the_render_key);
    RUN_TEST(test_fermentation_text_packs_are_complete_static_tables);
    RUN_TEST(test_device_name_editor_commits_through_the_owner_command);
    RUN_TEST(test_keyboard_follows_the_owning_text_rules_and_byte_limit);
    RUN_TEST(test_keyboard_backspace_keeps_a_multibyte_name_valid_utf8);
    RUN_TEST(test_program_editor_lists_the_local_fields_in_a_fixed_order);
    RUN_TEST(
        test_program_editor_cycles_drop_values_the_setting_makes_unexpected);
    RUN_TEST(test_program_editor_numeric_fields_use_the_program_validator);
    RUN_TEST(
        test_program_editor_name_and_notes_use_the_keyboard_and_save_the_candidate);
    RUN_TEST(test_program_editor_copy_and_new_take_a_request_name_only);
    RUN_TEST(test_text_edit_route_follows_the_device_name_caller);
    RUN_TEST(test_text_edit_route_follows_the_program_editor_caller);
    RUN_TEST(
        test_program_editor_edits_are_dirty_and_never_touch_the_stored_program);
    RUN_TEST(
        test_program_editor_discard_is_an_explicit_slot_and_drops_the_candidate);
    RUN_TEST(
        test_program_save_marks_the_editor_clean_only_after_the_owner_accepts);
    return UNITY_END();
}
