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
    // The typed payload was decided or rejected without entering an owning
    // mutation/persistence path.
    DecisionOnly,
    // The typed payload reached the owning application path. The actual
    // typed result is carried in commandResult.
    OwningOutcome,
};

struct WorkspacePressDispatchResult {
    WorkspacePressDispatchOutcome outcome{
        WorkspacePressDispatchOutcome::NoTypedPayload};
    // Set only when press.action was populated.
    std::optional<FermentationApplicationRequestStatus> prepareStatus;
    // Set only when prepareStatus == Prepared: confirmPrepared()'s own
    // result for the same request.
    std::optional<FermentationApplicationRequestStatus> confirmStatus;
    // Set only when press.resumeFallback was populated.
    std::optional<RunPersistenceResultStatus> resumeFallbackStatus;
    // The existing bridge result, including its DecisionOnly versus
    // OwningOutcome phase. This is the authoritative typed outcome.
    std::optional<FermentationUiCommandResult> commandResult;
    // Set only when press.verifyServicePin was populated (Issue #188 A).
    std::optional<LocalServicePinResult> servicePinResult;
};

// The single app-specific adapter that turns an already-typed #26
// FermentationUiWorkspacePress result into calls against the existing
// FermentationApplication/FermentationUiCommandBridge entry points
// (prepareEnvelope()/confirmPrepared()/applyConfirmedPrepared() for
// press.action, the existing resumeFallback bridge for press.resumeFallback).
// It introduces no new
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

// What the UI loop must do with the touch sample of this iteration. This is
// the single decision point shared by the firmware loop and its host tests:
// a held contact is processed; without a contact, only the release of an armed
// factory reset hold is delivered (cheap: no screen model, no clock copy), so a
// long press that was interrupted can never survive the pause (Issue #19).
enum class TouchLoopAction : std::uint8_t {
    None,
    ProcessContact,
    ReleaseFactoryResetHold,
};

[[nodiscard]] inline TouchLoopAction touchLoopAction(
    bool contactHeld, const FermentationUiSnapshot& snapshot) noexcept {
    if (contactHeld) return TouchLoopAction::ProcessContact;
    return snapshot.factoryReset.stage == FactoryResetStage::Hold
               ? TouchLoopAction::ReleaseFactoryResetHold
               : TouchLoopAction::None;
}

// Reports "no contact on the hold target" to the factory reset flow.
void releaseFactoryResetHold(FermentationApplication& application,
                             std::uint64_t monotonicMillis);

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

// The UI loop's allocation-free steady-state gate (R1 RAM plan, S4). It is the
// part of updateProductUi() that precedes the renderer and is shared by the
// firmware and the host tests: recycled snapshot, the one presentation copy
// (S3) and the pre-built ScreenRenderKey. In an unchanged visible state none
// of these steps allocates after warm-up, and no screen model is built.
// Residual allocations are event-bound: a changed snapshot, catalog or locale,
// the by-value access-point fetch for an actual redraw, and messages or text
// keys beyond the buffers already grown.
class UiRenderGate {
   public:
    // 1. Refresh the recycled snapshot and the presentation copy. The
    //    presentation copy is evicted while HeaderNetwork is the current page.
    void beginStep(FermentationApplication& application, bool networkPage) {
        application.refreshUiSnapshot(snapshot_);
        presentation_.update(networkPage, snapshot_.revisions, [&application] {
            return application.uiPresentationSource();
        });
    }

    // The same step, additionally handing the owner's visible device name to
    // the workspace (a read-only display copy for the settings page and the
    // editor prefill, never edited locally). It changes the workspace render
    // revision only when the name actually changed.
    void beginStep(FermentationApplication& application, bool networkPage,
                   FermentationTouchWorkspace& workspace) {
        beginStep(application, networkPage);
        if (presentation_.hasCopy()) {
            workspace.adoptDeviceName(presentation_.get().deviceName);
        }
    }

    // 2. After touch handling: refresh the presentation copy for the page
    //    that will be drawn, build the key and compare it with the last
    //    successfully rendered key. Returns true if a redraw is required.
    //    The access-point change revision is only read on HeaderNetwork and
    //    never copies SSID or password.
    [[nodiscard]] bool renderRequired(
        FermentationApplication& application,
        const FermentationTouchWorkspace& workspace,
        std::optional<device_platform::DeviceUiTarget> pressedTarget,
        device_platform::DeviceUiNetworkStatus networkStatus,
        std::optional<std::int64_t> trustedUtc) {
        const bool networkPage =
            workspace.page() == FermentationUiPage::HeaderNetwork;
        presentation_.update(networkPage, snapshot_.revisions, [&application] {
            return application.uiPresentationSource();
        });
        // The presentation cache keeps the last filled locale across the
        // HeaderNetwork eviction, so one accessor serves every page.
        const auto& locale = presentation_.displayLocale();
        pendingKey_ = makeScreenRenderKey(
            snapshot_, workspace, locale, pressedTarget, presentation_,
            networkStatus, trustedUtc,
            networkPage ? application.networkAccessPointRevision()
                        : std::uint64_t{0U});
        return !renderedKey_.has_value() || *renderedKey_ != *pendingKey_;
    }

    // 3. Call after a successful render() of the key last passed through
    //    renderRequired(); a failed render keeps the old key so the next loop
    //    tries again.
    void markRendered() { renderedKey_ = pendingKey_; }

    [[nodiscard]] const FermentationUiSnapshot& snapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] const FermentationUiPresentationCache& presentation()
        const noexcept {
        return presentation_;
    }

   private:
    FermentationUiSnapshot snapshot_;
    FermentationUiPresentationCache presentation_;
    std::optional<ScreenRenderKey> renderedKey_;
    std::optional<ScreenRenderKey> pendingKey_;
};

}  // namespace fermentation::main_ui
