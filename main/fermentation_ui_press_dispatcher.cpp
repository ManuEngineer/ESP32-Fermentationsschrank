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
    if (press.transitionAction.has_value() || press.programEdit.has_value()) {
        // No existing FermentationApplication entry point owns either of
        // these today:
        //  - ProductInsertedConfirmed's only existing handling
        //    (FermentationUiCommandBridge::decideProductInsertedConfirmed)
        //    takes private RunCommandState/ProcessSignals this composition
        //    boundary does not have access to;
        //  - program editing (Reset/Uninstall/Delete/SaveProgram) has no
        //    application-side catalog-mutation entry point at all yet.
        WorkspacePressDispatchResult unavailable;
        unavailable.outcome = WorkspacePressDispatchOutcome::UnavailableNoOwner;
        return unavailable;
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
    const auto screen =
        makeRepresentativeScreen(snapshot, workspace, textPacks, locale,
                                 std::nullopt, catalog, networkStatus, clock);
    result.pressedTarget = targetAt(screen, touchX, touchY);
    if (freshPressEdge && result.pressedTarget.has_value()) {
        const auto press =
            routePress(workspace, snapshot, screen, touchX, touchY, catalog);
        result.dispatch = dispatchWorkspacePress(application, snapshot, press,
                                                 monotonicMillis);
    }
    return result;
}

}  // namespace fermentation::main_ui
