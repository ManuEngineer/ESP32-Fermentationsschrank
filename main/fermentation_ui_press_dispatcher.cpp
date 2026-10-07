#include "fermentation_ui_press_dispatcher.hpp"

namespace fermentation::main_ui {

WorkspacePressDispatchResult dispatchWorkspacePress(
    FermentationApplication& application,
    const FermentationUiSnapshot& snapshot,
    const FermentationUiWorkspacePress& press, std::uint64_t monotonicMillis) {
    if (press.action.has_value()) {
        FermentationUiCommandContext context;
        context.surface = device_platform::UiSurface::LocalDisplay;
        context.monotonicMillis = monotonicMillis;
        context.expected = snapshot.revisions;
        // context.confirmed stays at its struct default (false): see the
        // header comment.
        const auto prepared =
            application.prepareEnvelope(context, *press.action);
        WorkspacePressDispatchResult result;
        result.prepareStatus = prepared.status;
        if (prepared.status == FermentationApplicationRequestStatus::Prepared) {
            const auto confirmed = application.confirmPrepared(prepared);
            result.confirmStatus = confirmed.status;
            if (confirmed.status ==
                    FermentationApplicationRequestStatus::Prepared &&
                confirmed.request.has_value()) {
                result.commandResult =
                    application.applyConfirmedPrepared(*confirmed.request);
                result.outcome =
                    result.commandResult->phase ==
                            FermentationUiCommandPhase::OwningOutcome
                        ? WorkspacePressDispatchOutcome::OwningOutcome
                        : WorkspacePressDispatchOutcome::DecisionOnly;
            } else {
                result.outcome = WorkspacePressDispatchOutcome::DecisionOnly;
            }
        } else {
            result.outcome = WorkspacePressDispatchOutcome::DecisionOnly;
        }
        return result;
    }
    if (press.applyNetworkMode.has_value()) {
        WorkspacePressDispatchResult result;
        result.commandResult = FermentationUiCommandBridge::applyNetworkMode(
            application, *press.applyNetworkMode);
        result.outcome = result.commandResult->phase ==
                                 FermentationUiCommandPhase::OwningOutcome
                             ? WorkspacePressDispatchOutcome::OwningOutcome
                             : WorkspacePressDispatchOutcome::DecisionOnly;
        return result;
    }
    if (press.beginHomeWifiReconfiguration.has_value()) {
        WorkspacePressDispatchResult result;
        result.commandResult =
            FermentationUiCommandBridge::beginHomeWifiReconfiguration(
                application, *press.beginHomeWifiReconfiguration);
        result.outcome = result.commandResult->phase ==
                                 FermentationUiCommandPhase::OwningOutcome
                             ? WorkspacePressDispatchOutcome::OwningOutcome
                             : WorkspacePressDispatchOutcome::DecisionOnly;
        return result;
    }
    if (press.setDeviceName.has_value()) {
        WorkspacePressDispatchResult result;
        result.commandResult = FermentationUiCommandBridge::setDeviceName(
            application, *press.setDeviceName);
        result.outcome = result.commandResult->phase ==
                                 FermentationUiCommandPhase::OwningOutcome
                             ? WorkspacePressDispatchOutcome::OwningOutcome
                             : WorkspacePressDispatchOutcome::DecisionOnly;
        return result;
    }
    if (press.setDisplayLanguage.has_value()) {
        WorkspacePressDispatchResult result;
        result.commandResult = FermentationUiCommandBridge::setDisplayLanguage(
            application, *press.setDisplayLanguage);
        result.outcome = result.commandResult->phase ==
                                 FermentationUiCommandPhase::OwningOutcome
                             ? WorkspacePressDispatchOutcome::OwningOutcome
                             : WorkspacePressDispatchOutcome::DecisionOnly;
        return result;
    }
    if (press.openWebProvisioningWindow.has_value()) {
        WorkspacePressDispatchResult result;
        result.commandResult =
            FermentationUiCommandBridge::openWebProvisioningWindow(
                application, *press.openWebProvisioningWindow);
        result.outcome = result.commandResult->phase ==
                                 FermentationUiCommandPhase::OwningOutcome
                             ? WorkspacePressDispatchOutcome::OwningOutcome
                             : WorkspacePressDispatchOutcome::DecisionOnly;
        return result;
    }
    if (press.resumeFallback.has_value()) {
        WorkspacePressDispatchResult dispatched;
        dispatched.commandResult = FermentationUiCommandBridge::resumeFallback(
            application, *press.resumeFallback);
        dispatched.outcome = dispatched.commandResult->phase ==
                                     FermentationUiCommandPhase::OwningOutcome
                                 ? WorkspacePressDispatchOutcome::OwningOutcome
                                 : WorkspacePressDispatchOutcome::DecisionOnly;
        if (std::holds_alternative<RunPersistenceResultStatus>(
                dispatched.commandResult->detail)) {
            dispatched.resumeFallbackStatus =
                std::get<RunPersistenceResultStatus>(
                    dispatched.commandResult->detail);
        }
        return dispatched;
    }
    if (press.transitionAction.has_value()) {
        // The UI carries only the intent; the expected state sequence comes
        // from the snapshot the user saw.  The application owns the process
        // decision and its persistence (Issue #172, S5).
        FermentationUiCommandContext context;
        context.surface = device_platform::UiSurface::LocalDisplay;
        context.monotonicMillis = monotonicMillis;
        context.expected = snapshot.revisions;
        WorkspacePressDispatchResult result;
        result.commandResult = application.confirmProductInserted(context);
        result.outcome = result.commandResult->phase ==
                                 FermentationUiCommandPhase::OwningOutcome
                             ? WorkspacePressDispatchOutcome::OwningOutcome
                             : WorkspacePressDispatchOutcome::DecisionOnly;
        return result;
    }
    if (press.programEdit.has_value()) {
        // The UI carries only the typed request; usage evidence and the
        // owning mutation stay in the application (Issue #172, S6).  Both
        // expected revisions are the ones the user saw.
        WorkspacePressDispatchResult result;
        result.commandResult = FermentationUiCommandBridge::applyProgramEdit(
            application, *press.programEdit,
            snapshot.revisions.expectedProgramCatalogRevision,
            snapshot.revisions.expectedUserConfigurationRevision);
        result.outcome = result.commandResult->phase ==
                                 FermentationUiCommandPhase::OwningOutcome
                             ? WorkspacePressDispatchOutcome::OwningOutcome
                             : WorkspacePressDispatchOutcome::DecisionOnly;
        return result;
    }
    return {};
}

WorkspaceTouchTickResult processWorkspaceTouch(
    FermentationApplication& application, FermentationTouchWorkspace& workspace,
    const FermentationUiSnapshot& snapshot,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale, const ProgramCatalog* catalog,
    device_platform::DeviceUiNetworkStatus networkStatus,
    device_platform::ClockViewInput clock, bool contactHeld,
    std::uint16_t touchX, std::uint16_t touchY, bool freshPressEdge,
    std::uint64_t monotonicMillis) {
    WorkspaceTouchTickResult result;
    if (!contactHeld) {
        return result;
    }
    // The screen is built once here and reused for both targetAt() and
    // routePress() (when a fresh press fires), so they use the same header and
    // bottom-slot geometry the user was actually looking at. This is
    // deliberately the same set of inputs render() itself uses to rebuild its
    // own screen for drawing, with pressedTarget left unset: this screen
    // represents the state as displayed *before* this press is routed.
    // Destroy its vector/string storage before a typed press can mutate owners.
    FermentationUiWorkspacePress press;
    bool shouldDispatch = false;
    {
        const auto screen = makeRepresentativeScreen(
            snapshot, workspace, textPacks, locale, std::nullopt, catalog,
            networkStatus, clock);
        result.pressedTarget = targetAt(screen, touchX, touchY);
        if (freshPressEdge && result.pressedTarget.has_value()) {
            press = routePress(workspace, snapshot, screen, touchX, touchY,
                               catalog);
            shouldDispatch = true;
        }
    }
    if (shouldDispatch) {
        result.dispatch = dispatchWorkspacePress(application, snapshot, press,
                                                 monotonicMillis);
        if (press.setDeviceName.has_value()) {
            // The settings page shows a refused name change; an accepted one
            // replaces an earlier failure (the name itself arrives through the
            // presentation copy).
            workspace.noteDeviceNameOutcome(
                result.dispatch.commandResult.has_value() &&
                result.dispatch.commandResult->category ==
                    device_platform::DeviceUiCommandOutcomeCategory::Accepted);
        }
        if (press.setDisplayLanguage.has_value()) {
            // The language page shows a refused change; an accepted one
            // replaces an earlier failure.
            workspace.noteDisplayLanguageOutcome(
                result.dispatch.commandResult.has_value() &&
                result.dispatch.commandResult->category ==
                    device_platform::DeviceUiCommandOutcomeCategory::Accepted);
        }
    }
    return result;
}

}  // namespace fermentation::main_ui
