#include <unity.h>

#include <algorithm>
#include <array>

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

FermentationApplicationOwningEvidence owningEvidence() {
    FermentationApplicationOwningEvidence evidence;
    evidence.safetyAllowsStart = true;
    evidence.airSensorValid = true;
    evidence.coolingSensorValid = true;
    evidence.productSensorValid = true;
    return evidence;
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
    const auto prepared = application.prepareEnvelope(
        context, *timedPress.action, owningEvidence());
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
    const std::array<const char*, 53U> keys{"standby",
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
                                            "program-invalid"};
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
    const auto copied = applyProgramEdit(catalog, *copy.programEdit);
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
    const auto created = applyProgramEdit(catalog, *newRequest.programEdit);
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
    const auto deleteOffer = workspace.press(snapshot, bottom(2), &catalog);
    TEST_ASSERT_TRUE(deleteOffer.navigated);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiPage::ProgramDeleteConfirmation),
        static_cast<int>(workspace.page()));
    const auto confirmedDelete = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Confirm, 0U}, &catalog);
    TEST_ASSERT_TRUE(confirmedDelete.programEdit.has_value());
    TEST_ASSERT_TRUE(confirmedDelete.programEdit->confirmed);
    const auto deleted =
        applyProgramEdit(catalog, *confirmedDelete.programEdit);
    TEST_ASSERT_TRUE(deleted.status ==
                     FermentationUiProgramEditStatus::Applied);
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
    RUN_TEST(test_sim_26_workspace_action_matrix_and_owner_paths);
    RUN_TEST(test_sim_26_navigation_and_non_command_slots);
    RUN_TEST(test_sim_26_manual_and_program_consumer_paths);
    RUN_TEST(test_sim_26_shell_locale_and_service_boundaries);
    RUN_TEST(test_sim_26_program_editor_actions_are_real_requests);
    RUN_TEST(test_sim_26_message_sensor_and_recovery_actions);
    return UNITY_END();
}
