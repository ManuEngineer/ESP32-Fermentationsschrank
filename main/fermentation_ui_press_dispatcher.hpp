#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "fermentation_application.hpp"
#include "fermentation_touch_workspace.hpp"
#include "fermentation_ui_renderer.hpp"

namespace fermentation::main_ui {

enum class WorkspacePressDispatchOutcome : std::uint8_t {
    // The press carried no typed command payload at all (pure navigation,
    // pager movement, or a slot with no typed result). Nothing to
    // dispatch; workspace-owned navigation already took effect.
    NoTypedPayload,
    // A typed payload was forwarded to its existing application entry
    // point. See the individual status fields for the outcome.
    Dispatched,
    // A typed payload exists (transitionAction or programEdit) but has no
    // existing FermentationApplication entry point today. This is reported
    // explicitly, not silently dropped - see the session handover for the
    // documented missing owner of each case.
    UnavailableNoOwner,
};

struct WorkspacePressDispatchResult {
    WorkspacePressDispatchOutcome outcome{
        WorkspacePressDispatchOutcome::NoTypedPayload};
    // Set only when outcome == Dispatched and press.action was populated.
    std::optional<FermentationApplicationRequestStatus> prepareStatus;
    // Set only when prepareStatus == Prepared: confirmPrepared()'s own
    // result for the same request.
    std::optional<FermentationApplicationRequestStatus> confirmStatus;
    // Set only when outcome == Dispatched and press.resumeFallback was
    // populated.
    std::optional<RunPersistenceResultStatus> resumeFallbackStatus;
};

// The single app-specific adapter that turns an already-typed #26
// FermentationUiWorkspacePress result into calls against the existing
// FermentationApplication/FermentationUiCommandBridge entry points
// (prepareEnvelope()/confirmPrepared() for press.action,
// resumeFallback() for press.resumeFallback). It introduces no new
// command bus and no new confirmation semantics: the
// FermentationUiCommandContext it builds leaves `confirmed` at its
// struct default (false), and press.resumeFallback's own `confirmed`
// field is forwarded exactly as the workspace produced it - the
// workspace's own navigation/dialog state is what already gated whether
// a typed payload appears at all.
[[nodiscard]] WorkspacePressDispatchResult dispatchWorkspacePress(
    FermentationApplication& application,
    const FermentationUiSnapshot& snapshot,
    const FermentationUiWorkspacePress& press, std::uint64_t monotonicMillis);

struct WorkspaceTouchTickResult {
    // The target under the current contact, for the caller to pass as
    // render()'s pressedTarget (nullopt when no contact is held, so
    // visible press feedback clears on release).
    std::optional<device_platform::DeviceUiTarget> pressedTarget;
    // Populated only on a fresh press edge that hit a valid target;
    // otherwise left at its NoTypedPayload default.
    WorkspacePressDispatchResult dispatch;
};

// The single app-specific adapter bridging a calibrated touch contact
// (already sampled and calibrated by the caller's display adapter) into
// the existing #26 targetAt()/Workspace::press() path and, on a fresh
// press, into dispatchWorkspacePress(). This is the only place
// targetAt()/routePress() are called from the composition root's touch
// path; main/app_main.cpp never builds a RepresentativeScreen or calls
// them directly. contactHeld/touchX/touchY describe the CURRENT contact
// state (every tick); freshPressEdge is true only on the tick a new press
// begins, and is when routePress()/dispatch actually run - every other
// tick only recomputes pressedTarget.
[[nodiscard]] WorkspaceTouchTickResult processWorkspaceTouch(
    FermentationApplication& application, FermentationTouchWorkspace& workspace,
    const FermentationUiSnapshot& snapshot,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale, const ProgramCatalog* catalog,
    device_platform::DeviceUiNetworkStatus networkStatus,
    device_platform::ClockViewInput clock, bool contactHeld,
    std::uint16_t touchX, std::uint16_t touchY, bool freshPressEdge,
    std::uint64_t monotonicMillis);

}  // namespace fermentation::main_ui
