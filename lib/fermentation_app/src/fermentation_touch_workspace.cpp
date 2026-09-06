#include "fermentation_touch_workspace.hpp"

#include <algorithm>

#include "fermentation_ui_text.hpp"

namespace fermentation {

FermentationUiSafeBootOwner safeBootOwnerFor(
    FermentationUiSafeBootTarget target) noexcept {
    switch (target) {
        case FermentationUiSafeBootTarget::PersistentFactoryReset:
            return FermentationUiSafeBootOwner::Issue57;
        case FermentationUiSafeBootTarget::RawTouchRecovery:
            return FermentationUiSafeBootOwner::Issue31;
        case FermentationUiSafeBootTarget::NetworkProvisioningRecovery:
            return FermentationUiSafeBootOwner::Issue89;
        case FermentationUiSafeBootTarget::DiagnosticsExport:
            return FermentationUiSafeBootOwner::Issue28;
        case FermentationUiSafeBootTarget::ResumeFallback:
            return FermentationUiSafeBootOwner::ExistingRecoveryPath;
    }
    return FermentationUiSafeBootOwner::Issue28;
}

device_platform::TextKey FermentationTouchWorkspace::key(const char* value) {
    return fermentationTextKey(value);
}

device_platform::BottomSlot FermentationTouchWorkspace::slot(const char* label,
                                                             bool enabled) {
    return {device_platform::BottomSlotKind::Action, key(label), enabled};
}

FermentationUiWorkspaceView FermentationTouchWorkspace::makeHomeView(
    const FermentationUiSnapshot& snapshot) const {
    FermentationUiWorkspaceView view;
    view.page = FermentationUiPage::Home;
    view.title = key("home");
    view.route.segments = {view.title};
    view.route.exitRequirement = device_platform::PageExitRequirement::None;
    view.completionLocked = false;
    switch (snapshot.home.mode) {
        case FermentationHomeMode::Standby:
            view.bottomSlots = {slot("start"), slot("programs"), slot("status"),
                                slot("service", snapshot.service.available)};
            break;
        case FermentationHomeMode::ActiveRun:
            view.bottomSlots = {slot("stop"), slot("programs"), slot("details"),
                                slot("status")};
            break;
        case FermentationHomeMode::Waiting:
            view.bottomSlots = {slot("continue"), slot("programs"),
                                slot("details"), slot("status")};
            break;
        case FermentationHomeMode::Completed:
            view.bottomSlots = {slot("ok"), slot("programs"), slot("details"),
                                slot("status")};
            break;
        case FermentationHomeMode::Recovery:
            view.bottomSlots = {
                slot("resume-fallback",
                     snapshot.recovery.mode ==
                         RecoveryViewMode::FallbackSelectionRequired),
                slot("programs"), slot("status"), slot("service", false)};
            break;
        case FermentationHomeMode::Restricted:
            view.bottomSlots = {slot("recovery", false), slot("programs"),
                                slot("status"), slot("service", false)};
            view.blockedReason = key("unavailable");
            break;
        case FermentationHomeMode::Unavailable:
            view.bottomSlots = {slot("unavailable", false),
                                slot("programs", false), slot("status"),
                                slot("service", false)};
            view.blockedReason = key("unavailable");
            break;
    }
    if (snapshot.home.mode == FermentationHomeMode::Waiting) {
        view.transitionAction = FermentationUiProductInsertedConfirmedIntent{};
    }
    view.action = homeAction(snapshot);
    return view;
}

std::optional<FermentationUiEnvelopePayload>
FermentationTouchWorkspace::homeAction(
    const FermentationUiSnapshot& snapshot) const {
    switch (snapshot.home.mode) {
        case FermentationHomeMode::ActiveRun:
            // The first press opens the stop choice. It does not create an
            // envelope; the selected option is built explicitly below.
            return std::nullopt;
        case FermentationHomeMode::Waiting:
            // ProductInsertedConfirmed is a later transition bridge input;
            // the empty intent is still identity-free and carries no stale
            // revision of its own.
            return std::nullopt;
        case FermentationHomeMode::Completed:
            return std::nullopt;
        case FermentationHomeMode::Standby:
        case FermentationHomeMode::Recovery:
        case FermentationHomeMode::Restricted:
        case FermentationHomeMode::Unavailable:
            return std::nullopt;
    }
    return std::nullopt;
}

FermentationUiWorkspaceView FermentationTouchWorkspace::makePageView(
    const FermentationUiSnapshot& snapshot,
    const ProgramCatalog* catalog) const {
    if (page_ == FermentationUiPage::Home) return makeHomeView(snapshot);
    FermentationUiWorkspaceView view;
    view.page = page_;
    view.route.segments = {key("home")};
    view.route.exitRequirement = device_platform::PageExitRequirement::None;
    view.bottomSlots = {slot("back"), slot("up", false), slot("down", false),
                        slot("status")};
    switch (page_) {
        case FermentationUiPage::ProgramList:
            view.title = key("programs");
            if (catalog != nullptr) {
                view.pager.itemCount = catalog->programs.size();
                view.pager.currentIndex = pager_.currentIndex;
                view.bottomSlots[1].enabled = view.pager.canMoveUp();
                view.bottomSlots[2].enabled = view.pager.canMoveDown();
            }
            break;
        case FermentationUiPage::ProgramSummary:
            view.title = key("start");
            view.route.segments.push_back(key("programs"));
            view.bottomSlots[1] = slot("cancel");
            view.bottomSlots[2] =
                slot("confirm", selectedProgramId_.has_value());
            if (selectedProgramId_.has_value()) {
                FermentationUiStartProgramIntent start;
                start.programId = *selectedProgramId_;
                view.action = FermentationUiEnvelopePayload{start};
            }
            break;
        case FermentationUiPage::Process:
            view.title = key("running");
            view.route.segments.push_back(key("running"));
            view.bottomSlots = {slot("stop"), slot("programs"), slot("details"),
                                slot("status")};
            break;
        case FermentationUiPage::Technical:
        case FermentationUiPage::Messages:
        case FermentationUiPage::MessageDetail:
        case FermentationUiPage::Status:
        case FermentationUiPage::Diagnostics:
            view.title = key("status");
            view.route.segments.push_back(key("status"));
            break;
        case FermentationUiPage::Completion:
            view.title = key("completed");
            view.route.segments.push_back(key("completed"));
            view.completionLocked = true;
            view.route.exitRequirement =
                device_platform::PageExitRequirement::CompletionLocked;
            view.bottomSlots[0] = slot("ok");
            break;
        case FermentationUiPage::Service:
        case FermentationUiPage::Pin:
            view.title = key("service");
            view.route.segments.push_back(key("service"));
            if (!snapshot.service.available) {
                view.blockedReason =
                    snapshot.service.unavailableReason.value_or(
                        key("service-locked"));
            }
            break;
        case FermentationUiPage::Recovery:
            view.title = key("recovery");
            view.route.segments.push_back(key("recovery"));
            break;
        case FermentationUiPage::Home:
            break;
        case FermentationUiPage::ProgramEdit:
            view.title = key("programs");
            view.route.segments.push_back(key("programs"));
            view.route.segments.push_back(key("details"));
            view.bottomSlots[2] = slot("confirm");
            break;
    }
    return view;
}

FermentationUiWorkspaceView FermentationTouchWorkspace::view(
    const FermentationUiSnapshot& snapshot,
    const ProgramCatalog* catalog) const {
    return makePageView(snapshot, catalog);
}

FermentationUiStartManualHoldingIntent
FermentationTouchWorkspace::makeManualHoldingIntent(
    const FermentationUiManualRunPlanValues& values) const {
    return {values};
}

FermentationUiStartManualTimedIntent
FermentationTouchWorkspace::makeManualTimedIntent(
    const ManualTimedRunValues& values) const {
    return {values};
}

FermentationUiStopRunIntent FermentationTouchWorkspace::makeStopIntent(
    StopOption option,
    const std::optional<FermentationUiManualRunPlanValues>& coolingPlan) const {
    FermentationUiStopRunIntent intent;
    intent.option = option;
    intent.coolingPlan = coolingPlan;
    return intent;
}

FermentationUiCompleteRunIntent
FermentationTouchWorkspace::makeCompletionIntent(
    bool startCooling,
    const std::optional<FermentationUiManualRunPlanValues>& coolingPlan) const {
    FermentationUiCompleteRunIntent intent;
    intent.startCooling = startCooling;
    intent.coolingPlan = coolingPlan;
    return intent;
}

bool FermentationTouchWorkspace::selectProgram(const std::string& programId,
                                               const ProgramCatalog& catalog) {
    const auto found =
        std::find_if(catalog.programs.begin(), catalog.programs.end(),
                     [&programId](const ProgramDocument& document) {
                         return document.program.id == programId &&
                                document.program.installed;
                     });
    if (found == catalog.programs.end()) return false;
    selectedProgramId_ = programId;
    page_ = FermentationUiPage::ProgramSummary;
    return true;
}

FermentationUiWorkspacePress FermentationTouchWorkspace::press(
    const FermentationUiSnapshot& snapshot,
    const device_platform::DeviceUiTarget& target,
    const ProgramCatalog* catalog) {
    const auto current = view(snapshot, catalog);
    FermentationUiWorkspacePress result;
    const bool enabled =
        target.kind == device_platform::DeviceUiTargetKind::BottomSlot &&
                target.slotIndex < current.bottomSlots.size()
            ? current.bottomSlots[target.slotIndex].enabled
            : target.valid();
    result.interaction = device_platform::selectDeviceUiTarget(
        {target, enabled, false, false, current.route.exitRequirement});
    if (result.interaction.outcome !=
        device_platform::DeviceUiInteractionOutcome::TargetSelected) {
        return result;
    }
    if (target.kind == device_platform::DeviceUiTargetKind::HomeOrBack) {
        auto route = current.route;
        result.navigated = device_platform::applyLocalNavigation(
            route, device_platform::homeOrBackAction(route));
        if (result.navigated) page_ = FermentationUiPage::Home;
    } else if (target.kind == device_platform::DeviceUiTargetKind::PagerUp) {
        result.navigated = pager_.moveUp();
    } else if (target.kind == device_platform::DeviceUiTargetKind::PagerDown) {
        result.navigated = pager_.moveDown();
    } else if (target.kind == device_platform::DeviceUiTargetKind::BottomSlot &&
               page_ == FermentationUiPage::Home) {
        if (target.slotIndex == 0U || target.slotIndex == 1U) {
            if (snapshot.home.mode == FermentationHomeMode::Waiting &&
                target.slotIndex == 0U) {
                result.transitionAction =
                    FermentationUiProductInsertedConfirmedIntent{};
            } else if (snapshot.home.mode == FermentationHomeMode::Standby) {
                page_ = FermentationUiPage::ProgramList;
                result.navigated = true;
            }
        } else {
            result.action = current.action;
        }
    } else {
        result.action = current.action;
    }
    return result;
}

}  // namespace fermentation
