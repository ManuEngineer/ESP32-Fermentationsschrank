#include "fermentation_touch_workspace.hpp"

#include <algorithm>
#include <utility>

#include "fermentation_ui_editing.hpp"
#include "fermentation_ui_text.hpp"

namespace fermentation {

namespace {

std::vector<FermentationUiSafeBootCapability>
safeBootUnavailableCapabilities() {
    return {FermentationUiSafeBootCapability::PersistentFactoryReset,
            FermentationUiSafeBootCapability::RawTouchRecovery,
            FermentationUiSafeBootCapability::NetworkProvisioningRecovery,
            FermentationUiSafeBootCapability::DiagnosticsExport};
}

}  // namespace

FermentationUiSafeBootCapability safeBootCapabilityFor(
    FermentationUiSafeBootTarget target) noexcept {
    switch (target) {
        case FermentationUiSafeBootTarget::PersistentFactoryReset:
            return FermentationUiSafeBootCapability::PersistentFactoryReset;
        case FermentationUiSafeBootTarget::RawTouchRecovery:
            return FermentationUiSafeBootCapability::RawTouchRecovery;
        case FermentationUiSafeBootTarget::NetworkProvisioningRecovery:
            return FermentationUiSafeBootCapability::
                NetworkProvisioningRecovery;
        case FermentationUiSafeBootTarget::DiagnosticsExport:
            return FermentationUiSafeBootCapability::DiagnosticsExport;
        case FermentationUiSafeBootTarget::ResumeFallback:
            return FermentationUiSafeBootCapability::ExistingRecoveryPath;
    }
    return FermentationUiSafeBootCapability::DiagnosticsExport;
}

device_platform::TextKey FermentationTouchWorkspace::key(const char* value) {
    return fermentationTextKey(value);
}

device_platform::BottomSlot FermentationTouchWorkspace::slot(const char* label,
                                                             bool enabled) {
    return {device_platform::BottomSlotKind::Action, key(label), enabled};
}

bool FermentationTouchWorkspace::isPageExitAction(
    FermentationUiWorkspaceSlotAction action) noexcept {
    switch (action) {
        case FermentationUiWorkspaceSlotAction::NavigateBack:
        case FermentationUiWorkspaceSlotAction::NavigateHome:
            return true;
        case FermentationUiWorkspaceSlotAction::None:
        case FermentationUiWorkspaceSlotAction::NavigateProgramList:
        case FermentationUiWorkspaceSlotAction::NavigateProgramManagement:
        case FermentationUiWorkspaceSlotAction::NavigateProgramSummary:
        case FermentationUiWorkspaceSlotAction::NavigateProgramEdit:
        case FermentationUiWorkspaceSlotAction::
            NavigateProgramDeleteConfirmation:
        case FermentationUiWorkspaceSlotAction::
            NavigateProgramDeleteFinalConfirmation:
        case FermentationUiWorkspaceSlotAction::NavigateProgramActions:
        case FermentationUiWorkspaceSlotAction::NavigateManualModeSelection:
        case FermentationUiWorkspaceSlotAction::NavigateManualHolding:
        case FermentationUiWorkspaceSlotAction::NavigateManualTimed:
        case FermentationUiWorkspaceSlotAction::NavigateProcess:
        case FermentationUiWorkspaceSlotAction::NavigateStopDialog:
        case FermentationUiWorkspaceSlotAction::NavigateTechnical:
        case FermentationUiWorkspaceSlotAction::NavigateMessages:
        case FermentationUiWorkspaceSlotAction::NavigateMessageDetail:
        case FermentationUiWorkspaceSlotAction::NavigateCompletion:
        case FermentationUiWorkspaceSlotAction::NavigateStatus:
        case FermentationUiWorkspaceSlotAction::NavigateDiagnostics:
        case FermentationUiWorkspaceSlotAction::NavigateService:
        case FermentationUiWorkspaceSlotAction::NavigatePin:
        case FermentationUiWorkspaceSlotAction::NavigateRecovery:
        case FermentationUiWorkspaceSlotAction::NavigateLanguage:
        case FermentationUiWorkspaceSlotAction::NavigateNetwork:
        case FermentationUiWorkspaceSlotAction::NavigateClock:
        case FermentationUiWorkspaceSlotAction::ApplyNetworkModeApOnly:
        case FermentationUiWorkspaceSlotAction::ApplyNetworkModeHomeWifi:
        case FermentationUiWorkspaceSlotAction::BeginHomeWifiReconfiguration:
        case FermentationUiWorkspaceSlotAction::NavigateWebAccess:
        case FermentationUiWorkspaceSlotAction::OpenWebProvisioningWindow:
        case FermentationUiWorkspaceSlotAction::MovePagerUp:
        case FermentationUiWorkspaceSlotAction::MovePagerDown:
        case FermentationUiWorkspaceSlotAction::BeginProgramEdit:
        case FermentationUiWorkspaceSlotAction::CopyProgram:
        case FermentationUiWorkspaceSlotAction::NewProgram:
        case FermentationUiWorkspaceSlotAction::ResetProgram:
        case FermentationUiWorkspaceSlotAction::UninstallProgram:
        case FermentationUiWorkspaceSlotAction::DeleteProgram:
        case FermentationUiWorkspaceSlotAction::SaveProgram:
        case FermentationUiWorkspaceSlotAction::ApplySensorSelection:
        case FermentationUiWorkspaceSlotAction::ApplyRecoveryTimeCorrection:
        case FermentationUiWorkspaceSlotAction::StartProgram:
        case FermentationUiWorkspaceSlotAction::StartManualHolding:
        case FermentationUiWorkspaceSlotAction::StartManualTimed:
        case FermentationUiWorkspaceSlotAction::ProductInsertedConfirmed:
        case FermentationUiWorkspaceSlotAction::StopTurnOff:
        case FermentationUiWorkspaceSlotAction::StopAndCool:
        case FermentationUiWorkspaceSlotAction::Complete:
        case FermentationUiWorkspaceSlotAction::CompleteAndCool:
        case FermentationUiWorkspaceSlotAction::ResumeFallback:
        case FermentationUiWorkspaceSlotAction::AcknowledgeMessage:
        case FermentationUiWorkspaceSlotAction::MuteMessage:
        case FermentationUiWorkspaceSlotAction::ResetFault:
            return false;
    }
    return false;
}

std::vector<device_platform::TextKey> FermentationTouchWorkspace::routeForPage(
    FermentationUiPage page) {
    std::vector<device_platform::TextKey> route{key("home")};
    switch (page) {
        case FermentationUiPage::Home:
        case FermentationUiPage::ProgramList:
            break;
        case FermentationUiPage::ProgramSummary:
        case FermentationUiPage::ProgramActions:
            route.push_back(key("programs"));
            break;
        case FermentationUiPage::ProgramEdit:
            route.push_back(key("programs"));
            route.push_back(key("details"));
            break;
        case FermentationUiPage::ProgramDeleteConfirmation:
            route.push_back(key("programs"));
            route.push_back(key("details"));
            route.push_back(key("delete"));
            break;
        case FermentationUiPage::ProgramDeleteFinalConfirmation:
            route.push_back(key("programs"));
            route.push_back(key("details"));
            route.push_back(key("delete"));
            route.push_back(key("confirm"));
            break;
        case FermentationUiPage::ManualModeSelection:
            route.push_back(key("programs"));
            route.push_back(key("manual"));
            break;
        case FermentationUiPage::ManualHolding:
        case FermentationUiPage::ManualTimed:
            route.push_back(key("programs"));
            route.push_back(key("manual"));
            route.push_back(key("details"));
            break;
        case FermentationUiPage::Process:
        case FermentationUiPage::StopDialog:
            route.push_back(key("running"));
            break;
        case FermentationUiPage::Technical:
            route.push_back(key("status"));
            route.push_back(key("technical"));
            break;
        case FermentationUiPage::Messages:
            route.push_back(key("status"));
            route.push_back(key("messages"));
            break;
        case FermentationUiPage::MessageDetail:
            route.push_back(key("status"));
            route.push_back(key("messages"));
            route.push_back(key("details"));
            break;
        case FermentationUiPage::Completion:
            route.push_back(key("completed"));
            break;
        case FermentationUiPage::Status:
            route.push_back(key("status"));
            break;
        case FermentationUiPage::Diagnostics:
            route.push_back(key("status"));
            route.push_back(key("diagnostics"));
            break;
        case FermentationUiPage::Service:
            route.push_back(key("service"));
            break;
        case FermentationUiPage::Pin:
            route.push_back(key("service"));
            route.push_back(key("pin"));
            break;
        case FermentationUiPage::Recovery:
            route.push_back(key("recovery"));
            break;
        case FermentationUiPage::HeaderLanguage:
            route.push_back(key("language"));
            break;
        case FermentationUiPage::HeaderNetwork:
            route.push_back(key("network"));
            break;
        case FermentationUiPage::HeaderClock:
            route.push_back(key("clock"));
            break;
        case FermentationUiPage::HeaderWebAccess:
            route.push_back(key("web-access"));
            break;
    }
    return route;
}

void FermentationTouchWorkspace::setCanonicalPageStack(
    FermentationUiPage page) {
    pageStack_.clear();
    pageStack_.push_back(FermentationUiPage::Home);
    switch (page) {
        case FermentationUiPage::Home:
            break;
        case FermentationUiPage::ProgramList:
            pageStack_.push_back(FermentationUiPage::ProgramList);
            break;
        case FermentationUiPage::ProgramSummary:
            pageStack_.insert(pageStack_.end(),
                              {FermentationUiPage::ProgramList,
                               FermentationUiPage::ProgramSummary});
            break;
        case FermentationUiPage::ProgramEdit: {
            pageStack_.insert(pageStack_.end(),
                              {FermentationUiPage::ProgramList,
                               FermentationUiPage::ProgramSummary,
                               FermentationUiPage::ProgramEdit});
            break;
        }
        case FermentationUiPage::ProgramDeleteConfirmation:
            pageStack_.insert(pageStack_.end(),
                              {FermentationUiPage::ProgramList,
                               FermentationUiPage::ProgramSummary,
                               FermentationUiPage::ProgramEdit,
                               FermentationUiPage::ProgramDeleteConfirmation});
            break;
        case FermentationUiPage::ProgramDeleteFinalConfirmation:
            pageStack_.insert(
                pageStack_.end(),
                {FermentationUiPage::ProgramList,
                 FermentationUiPage::ProgramSummary,
                 FermentationUiPage::ProgramEdit,
                 FermentationUiPage::ProgramDeleteConfirmation,
                 FermentationUiPage::ProgramDeleteFinalConfirmation});
            break;
        case FermentationUiPage::ProgramActions:
            pageStack_.insert(pageStack_.end(),
                              {FermentationUiPage::ProgramList,
                               FermentationUiPage::ProgramActions});
            break;
        case FermentationUiPage::ManualModeSelection:
            pageStack_.insert(pageStack_.end(),
                              {FermentationUiPage::ProgramList,
                               FermentationUiPage::ManualModeSelection});
            break;
        case FermentationUiPage::ManualHolding:
        case FermentationUiPage::ManualTimed:
            pageStack_.insert(pageStack_.end(),
                              {FermentationUiPage::ProgramList,
                               FermentationUiPage::ManualModeSelection, page});
            break;
        case FermentationUiPage::Process:
            pageStack_.push_back(FermentationUiPage::Process);
            break;
        case FermentationUiPage::StopDialog:
            pageStack_.insert(
                pageStack_.end(),
                {FermentationUiPage::Process, FermentationUiPage::StopDialog});
            break;
        case FermentationUiPage::Technical:
        case FermentationUiPage::Messages:
        case FermentationUiPage::Diagnostics:
        case FermentationUiPage::Status:
            pageStack_.push_back(FermentationUiPage::Status);
            if (page != FermentationUiPage::Status) pageStack_.push_back(page);
            break;
        case FermentationUiPage::MessageDetail:
            pageStack_.insert(
                pageStack_.end(),
                {FermentationUiPage::Status, FermentationUiPage::Messages,
                 FermentationUiPage::MessageDetail});
            break;
        case FermentationUiPage::Completion:
            pageStack_.push_back(FermentationUiPage::Completion);
            break;
        case FermentationUiPage::Service:
            pageStack_.push_back(FermentationUiPage::Service);
            break;
        case FermentationUiPage::Pin:
            pageStack_.insert(pageStack_.end(), {FermentationUiPage::Service,
                                                 FermentationUiPage::Pin});
            break;
        case FermentationUiPage::Recovery:
            pageStack_.push_back(FermentationUiPage::Recovery);
            break;
        case FermentationUiPage::HeaderLanguage:
        case FermentationUiPage::HeaderNetwork:
        case FermentationUiPage::HeaderClock:
        case FermentationUiPage::HeaderWebAccess:
            pageStack_.push_back(page);
            break;
    }
    page_ = page;
}

void FermentationTouchWorkspace::setSlot(
    FermentationUiWorkspaceView& view, std::size_t index, const char* label,
    FermentationUiWorkspaceSlotAction action, bool enabled) const {
    if (index >= view.bottomSlots.size()) return;
    view.bottomSlots[index] = slot(label, enabled);
    view.slotActions[index] = action;
}

FermentationUiWorkspaceView FermentationTouchWorkspace::makeHomeView(
    const FermentationUiSnapshot& snapshot) const {
    FermentationUiWorkspaceView view;
    view.page = FermentationUiPage::Home;
    view.title = key("home");
    view.route.segments = routeForPage(FermentationUiPage::Home);

    switch (snapshot.home.mode) {
        case FermentationHomeMode::Standby:
            setSlot(view, 0U, "start",
                    FermentationUiWorkspaceSlotAction::NavigateProgramList);
            setSlot(
                view, 1U, "programs",
                FermentationUiWorkspaceSlotAction::NavigateProgramManagement);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "service",
                    FermentationUiWorkspaceSlotAction::NavigateService,
                    snapshot.service.available);
            if (!snapshot.service.available)
                view.blockedReason = snapshot.service.unavailableReason;
            break;
        case FermentationHomeMode::ActiveRun:
            setSlot(view, 0U, "stop",
                    FermentationUiWorkspaceSlotAction::NavigateStopDialog);
            setSlot(view, 1U, "programs",
                    FermentationUiWorkspaceSlotAction::NavigateProgramList);
            setSlot(view, 2U, "details",
                    FermentationUiWorkspaceSlotAction::NavigateProcess);
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationHomeMode::Waiting:
            if (snapshot.home.processState == ProcessState::WaitingForProduct) {
                setSlot(view, 0U, "continue",
                        FermentationUiWorkspaceSlotAction::
                            ProductInsertedConfirmed);
                view.transitionAction =
                    FermentationUiProductInsertedConfirmedIntent{};
            } else {
                // A UserDecisionRequired message is not a product-insert
                // event. Its existing message action remains on the message
                // owner path; the home slot only opens that path.
                setSlot(
                    view, 0U,
                    snapshot.home.primaryAction.valid()
                        ? snapshot.home.primaryAction.value.c_str()
                        : "messages",
                    FermentationUiWorkspaceSlotAction::NavigateMessageDetail);
            }
            setSlot(view, 1U, "programs",
                    FermentationUiWorkspaceSlotAction::NavigateProgramList);
            setSlot(view, 2U, "details",
                    FermentationUiWorkspaceSlotAction::NavigateProcess);
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationHomeMode::Completed:
            setSlot(view, 0U, "ok",
                    FermentationUiWorkspaceSlotAction::Complete);
            setSlot(view, 1U, "programs",
                    FermentationUiWorkspaceSlotAction::NavigateProgramList);
            setSlot(view, 2U, "details",
                    FermentationUiWorkspaceSlotAction::NavigateCompletion);
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationHomeMode::Recovery:
            if (snapshot.recovery.mode ==
                    RecoveryViewMode::FallbackSelectionRequired &&
                snapshot.home.processState != ProcessState::SafeBoot) {
                setSlot(view, 0U, "resume-fallback",
                        FermentationUiWorkspaceSlotAction::ResumeFallback);
            } else {
                setSlot(view, 0U, "recovery",
                        FermentationUiWorkspaceSlotAction::NavigateRecovery,
                        snapshot.home.processState != ProcessState::SafeBoot);
                if (snapshot.home.processState == ProcessState::SafeBoot)
                    view.unavailableCapabilities =
                        safeBootUnavailableCapabilities();
            }
            setSlot(view, 1U, "programs",
                    FermentationUiWorkspaceSlotAction::NavigateProgramList,
                    snapshot.home.processState != ProcessState::SafeBoot);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "service",
                    FermentationUiWorkspaceSlotAction::NavigateService, false);
            break;
        case FermentationHomeMode::Restricted:
            setSlot(
                view, 0U, "recovery",
                FermentationUiWorkspaceSlotAction::NavigateRecovery,
                snapshot.home.processState == ProcessState::RecoveryEvaluation);
            setSlot(view, 1U, "programs",
                    FermentationUiWorkspaceSlotAction::NavigateProgramList,
                    snapshot.home.processState != ProcessState::SafeBoot);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "service",
                    FermentationUiWorkspaceSlotAction::NavigateService, false);
            view.blockedReason = key("restricted");
            if (snapshot.home.processState == ProcessState::SafeBoot)
                view.unavailableCapabilities =
                    safeBootUnavailableCapabilities();
            break;
        case FermentationHomeMode::Unavailable:
            setSlot(view, 0U, "unavailable",
                    FermentationUiWorkspaceSlotAction::None, false);
            setSlot(view, 1U, "programs",
                    FermentationUiWorkspaceSlotAction::None, false);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "service",
                    FermentationUiWorkspaceSlotAction::None, false);
            view.blockedReason = key("unavailable");
            break;
    }
    return view;
}

FermentationUiWorkspaceView FermentationTouchWorkspace::makePageView(
    const FermentationUiSnapshot& snapshot,
    const ProgramCatalog* catalog) const {
    if (page_ == FermentationUiPage::Home) return makeHomeView(snapshot);

    FermentationUiWorkspaceView view;
    view.page = page_;
    view.route.segments = routeForPage(page_);
    setSlot(view, 0U, "back", FermentationUiWorkspaceSlotAction::NavigateBack);
    setSlot(view, 1U, "up", FermentationUiWorkspaceSlotAction::None, false);
    setSlot(view, 2U, "down", FermentationUiWorkspaceSlotAction::None, false);
    setSlot(view, 3U, "status",
            FermentationUiWorkspaceSlotAction::NavigateStatus);

    switch (page_) {
        case FermentationUiPage::ProgramList:
            view.title = key(programListIntent_ ==
                                     FermentationUiProgramListIntent::Manage
                                 ? "programs"
                                 : "start");
            if (catalog != nullptr) {
                view.programList = makeFermentationUiProgramList(*catalog);
                view.pager.itemCount = view.programList.size();
            }
            view.pager.currentIndex = pager_.currentIndex;
            setSlot(view, 0U, "home",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "up",
                    FermentationUiWorkspaceSlotAction::MovePagerUp,
                    view.pager.canMoveUp());
            setSlot(view, 2U, "down",
                    FermentationUiWorkspaceSlotAction::MovePagerDown,
                    view.pager.canMoveDown());
            // Manual operation belongs to the start path; the management list
            // keeps the default status slot.
            if (programListIntent_ == FermentationUiProgramListIntent::Start) {
                setSlot(view, 3U, "manual",
                        FermentationUiWorkspaceSlotAction::
                            NavigateManualModeSelection);
            }
            break;
        case FermentationUiPage::ProgramSummary:
            view.title = key("start");
            // A listed program that cannot be started stays selectable for
            // administration; the owning list projection names the reason.
            if (catalog != nullptr && selectedProgramId_.has_value()) {
                const auto entries = makeFermentationUiProgramList(*catalog);
                const auto selected = std::find_if(
                    entries.begin(), entries.end(),
                    [this](const FermentationUiProgramListEntry& entry) {
                        return entry.program.program.id == *selectedProgramId_;
                    });
                if (selected != entries.end() && !selected->startable)
                    view.blockedReason = selected->blockedReason;
            }
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "edit",
                    FermentationUiWorkspaceSlotAction::NavigateProgramActions);
            setSlot(view, 2U, "confirm",
                    FermentationUiWorkspaceSlotAction::StartProgram,
                    selectedProgramId_.has_value() &&
                        selectedCandidate_.programId == *selectedProgramId_);
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::ProgramActions:
            view.title = key("program-actions");
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "edit",
                    FermentationUiWorkspaceSlotAction::BeginProgramEdit,
                    selectedProgramId_.has_value());
            setSlot(view, 2U, "copy",
                    FermentationUiWorkspaceSlotAction::CopyProgram,
                    selectedProgramId_.has_value());
            setSlot(view, 3U, "new",
                    FermentationUiWorkspaceSlotAction::NewProgram);
            break;
        case FermentationUiPage::ProgramEdit: {
            view.title = key("program-edit");
            view.route.exitRequirement =
                programEditDirty_
                    ? device_platform::PageExitRequirement::ConfirmDiscard
                    : device_platform::PageExitRequirement::None;
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            bool factoryProgram = false;
            bool resettableProgram = false;
            bool deletableProgram = selectedProgramId_.has_value();
            if (catalog != nullptr && selectedProgramId_.has_value()) {
                const auto found = std::find_if(
                    catalog->programs.begin(), catalog->programs.end(),
                    [this](const auto& document) {
                        return document.program.id == *selectedProgramId_;
                    });
                factoryProgram = found != catalog->programs.end() &&
                                 found->program.factoryCatalogEntry;
                resettableProgram = factoryProgram && found->program.resettable;
                deletableProgram = found != catalog->programs.end() &&
                                   found->program.installed &&
                                   found->program.userDeletable;
            }
            setSlot(view, 1U, "reset",
                    FermentationUiWorkspaceSlotAction::ResetProgram,
                    selectedProgramId_.has_value() && resettableProgram);
            setSlot(view, 2U, "delete",
                    factoryProgram
                        ? FermentationUiWorkspaceSlotAction::UninstallProgram
                        : FermentationUiWorkspaceSlotAction::DeleteProgram,
                    deletableProgram);
            setSlot(view, 3U, "save",
                    FermentationUiWorkspaceSlotAction::SaveProgram,
                    programEditOperation_ !=
                            FermentationUiProgramEditOperation::Edit ||
                        programEditCandidate_.has_value());
            break;
        }
        case FermentationUiPage::ProgramDeleteConfirmation:
        case FermentationUiPage::ProgramDeleteFinalConfirmation:
            view.title = key("delete");
            if (catalog != nullptr && selectedProgramId_.has_value()) {
                const auto found = std::find_if(
                    catalog->programs.begin(), catalog->programs.end(),
                    [this](const auto& document) {
                        return document.program.id == *selectedProgramId_;
                    });
                if (found != catalog->programs.end())
                    view.confirmationProgramName = found->program.name;
                if (found != catalog->programs.end() &&
                    found->program.builtIn &&
                    found->program.factoryCatalogEntry)
                    view.confirmationWarning = key("factory-reset-required");
            }
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            if (page_ == FermentationUiPage::ProgramDeleteConfirmation) {
                setSlot(view, 1U, "confirm",
                        FermentationUiWorkspaceSlotAction::
                            NavigateProgramDeleteFinalConfirmation,
                        selectedProgramId_.has_value());
                setSlot(view, 2U, "cancel",
                        FermentationUiWorkspaceSlotAction::NavigateBack);
            } else {
                setSlot(view, 1U, "cancel",
                        FermentationUiWorkspaceSlotAction::NavigateBack);
                setSlot(view, 2U, "status",
                        FermentationUiWorkspaceSlotAction::NavigateStatus);
                setSlot(
                    view, 3U, "delete",
                    programEditOperation_ ==
                            FermentationUiProgramEditOperation::Uninstall
                        ? FermentationUiWorkspaceSlotAction::UninstallProgram
                        : FermentationUiWorkspaceSlotAction::DeleteProgram,
                    selectedProgramId_.has_value());
            }
            if (page_ == FermentationUiPage::ProgramDeleteConfirmation)
                setSlot(view, 3U, "status",
                        FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::ManualModeSelection:
            view.title = key("manual");
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "manual-holding",
                    FermentationUiWorkspaceSlotAction::NavigateManualHolding);
            setSlot(view, 2U, "manual-timed",
                    FermentationUiWorkspaceSlotAction::NavigateManualTimed);
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::ManualHolding:
            view.title = key("manual-holding");
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "cancel",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 2U, "confirm",
                    FermentationUiWorkspaceSlotAction::StartManualHolding,
                    manualHoldingValues_.has_value());
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::ManualTimed:
            view.title = key("manual-timed");
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "cancel",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 2U, "confirm",
                    FermentationUiWorkspaceSlotAction::StartManualTimed,
                    manualTimedValues_.has_value());
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::Process:
            view.title = key("running");
            setSlot(view, 0U, "stop",
                    FermentationUiWorkspaceSlotAction::NavigateStopDialog);
            setSlot(view, 1U, "programs",
                    FermentationUiWorkspaceSlotAction::NavigateProgramList);
            setSlot(view, 2U, "technical",
                    FermentationUiWorkspaceSlotAction::NavigateTechnical);
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::StopDialog:
            view.title = key("stop");
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "stop-turn-off",
                    FermentationUiWorkspaceSlotAction::StopTurnOff);
            setSlot(view, 2U, "stop-and-cool",
                    FermentationUiWorkspaceSlotAction::StopAndCool,
                    stopCoolingPlan_.has_value());
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::Technical:
            view.title = key("technical");
            setSlot(view, 1U, "up",
                    FermentationUiWorkspaceSlotAction::MovePagerUp,
                    view.pager.canMoveUp());
            setSlot(view, 2U, "down",
                    FermentationUiWorkspaceSlotAction::MovePagerDown,
                    view.pager.canMoveDown());
            setSlot(view, 3U, "messages",
                    FermentationUiWorkspaceSlotAction::NavigateMessages);
            break;
        case FermentationUiPage::Messages:
            view.title = key("messages");
            view.pager.itemCount = snapshot.messages.size();
            view.pager.currentIndex = pager_.currentIndex;
            setSlot(view, 1U, "up",
                    FermentationUiWorkspaceSlotAction::MovePagerUp,
                    view.pager.canMoveUp());
            setSlot(view, 2U, "down",
                    FermentationUiWorkspaceSlotAction::MovePagerDown,
                    view.pager.canMoveDown());
            setSlot(view, 3U, "details",
                    FermentationUiWorkspaceSlotAction::NavigateMessageDetail,
                    !snapshot.messages.empty());
            break;
        case FermentationUiPage::MessageDetail:
            view.title = key("message-detail");
            setSlot(view, 1U, "acknowledge",
                    FermentationUiWorkspaceSlotAction::AcknowledgeMessage,
                    selectedMessageId_.has_value());
            setSlot(view, 2U, "mute",
                    FermentationUiWorkspaceSlotAction::MuteMessage,
                    selectedMessageId_.has_value());
            if (sensorSelectionAction_.has_value()) {
                setSlot(
                    view, 3U, "continue",
                    FermentationUiWorkspaceSlotAction::ApplySensorSelection);
            } else {
                setSlot(view, 3U, "fault-reset",
                        FermentationUiWorkspaceSlotAction::ResetFault,
                        snapshot.home.processState == ProcessState::Fault);
            }
            break;
        case FermentationUiPage::Completion:
            view.title = key("completed");
            view.completionLocked = true;
            view.route.exitRequirement =
                device_platform::PageExitRequirement::CompletionLocked;
            setSlot(view, 1U, "details",
                    FermentationUiWorkspaceSlotAction::NavigateTechnical);
            setSlot(view, 2U, "ok",
                    FermentationUiWorkspaceSlotAction::Complete);
            setSlot(view, 3U, "cool-now",
                    FermentationUiWorkspaceSlotAction::CompleteAndCool,
                    completionCoolingPlan_.has_value());
            break;
        case FermentationUiPage::Status:
            view.title = key("status");
            setSlot(view, 1U, "messages",
                    FermentationUiWorkspaceSlotAction::NavigateMessages);
            setSlot(view, 2U, "diagnostics",
                    FermentationUiWorkspaceSlotAction::NavigateDiagnostics);
            setSlot(view, 3U, "technical",
                    FermentationUiWorkspaceSlotAction::NavigateTechnical);
            break;
        case FermentationUiPage::Diagnostics:
            view.title = key("diagnostics");
            setSlot(view, 1U, "up",
                    FermentationUiWorkspaceSlotAction::MovePagerUp,
                    view.pager.canMoveUp());
            setSlot(view, 2U, "down",
                    FermentationUiWorkspaceSlotAction::MovePagerDown,
                    view.pager.canMoveDown());
            setSlot(view, 3U, "service",
                    FermentationUiWorkspaceSlotAction::NavigateService);
            break;
        case FermentationUiPage::Service:
            view.title = key("service");
            if (!snapshot.service.available)
                view.blockedReason =
                    snapshot.service.unavailableReason.value_or(
                        key("service-locked"));
            setSlot(view, 1U, "pin",
                    FermentationUiWorkspaceSlotAction::NavigatePin,
                    snapshot.service.available);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "recovery",
                    FermentationUiWorkspaceSlotAction::NavigateRecovery,
                    snapshot.service.available);
            break;
        case FermentationUiPage::Pin:
            view.title = key("pin");
            setSlot(view, 1U, "cancel",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "service",
                    FermentationUiWorkspaceSlotAction::NavigateService);
            break;
        case FermentationUiPage::Recovery: {
            view.title = key("recovery");
            const bool fallbackAllowed =
                snapshot.recovery.mode ==
                    RecoveryViewMode::FallbackSelectionRequired &&
                snapshot.home.processState != ProcessState::SafeBoot;
            if (recoveryTimeCorrectionSeconds_.has_value() &&
                !fallbackAllowed) {
                setSlot(view, 1U, "confirm",
                        FermentationUiWorkspaceSlotAction::
                            ApplyRecoveryTimeCorrection);
            } else {
                setSlot(view, 1U, "resume-fallback",
                        FermentationUiWorkspaceSlotAction::ResumeFallback,
                        fallbackAllowed);
            }
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "diagnostics",
                    FermentationUiWorkspaceSlotAction::NavigateDiagnostics);
            if (!fallbackAllowed &&
                snapshot.recovery.mode ==
                    RecoveryViewMode::FallbackSelectionRequired) {
                view.unavailableCapabilities =
                    safeBootUnavailableCapabilities();
            }
            break;
        }
        case FermentationUiPage::HeaderLanguage:
            view.title = key("language");
            setSlot(view, 1U, "network",
                    FermentationUiWorkspaceSlotAction::NavigateNetwork);
            setSlot(view, 2U, "clock",
                    FermentationUiWorkspaceSlotAction::NavigateClock);
            setSlot(view, 3U, "web-access",
                    FermentationUiWorkspaceSlotAction::NavigateWebAccess);
            break;
        case FermentationUiPage::HeaderNetwork:
            view.title = key("network");
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "network-ap-only",
                    FermentationUiWorkspaceSlotAction::ApplyNetworkModeApOnly,
                    snapshot.network.currentMode !=
                        device_platform::NetworkMode::AP_ONLY);
            setSlot(view, 2U, "network-home-wifi",
                    FermentationUiWorkspaceSlotAction::ApplyNetworkModeHomeWifi,
                    snapshot.network.currentMode !=
                        device_platform::NetworkMode::HOME_WIFI);
            setSlot(
                view, 3U, "network-reconfigure",
                FermentationUiWorkspaceSlotAction::BeginHomeWifiReconfiguration,
                snapshot.network.currentMode ==
                    device_platform::NetworkMode::HOME_WIFI);
            break;
        case FermentationUiPage::HeaderWebAccess:
            view.title = key("web-access");
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            // Enabled only while the Application reports a possible but not
            // yet released setup; the Application re-validates on the press.
            setSlot(
                view, 1U, "web-access-open",
                FermentationUiWorkspaceSlotAction::OpenWebProvisioningWindow,
                snapshot.webAccess == FermentationWebAccessState::Closed);
            // The reason mirrors the Application-reported state; the
            // workspace adds no state logic of its own.
            if (snapshot.webAccess == FermentationWebAccessState::WindowOpen) {
                view.blockedReason = key("web-access-window-open");
            } else if (snapshot.webAccess ==
                       FermentationWebAccessState::NotApplicable) {
                view.blockedReason = key("web-access-unavailable");
            }
            break;
        case FermentationUiPage::HeaderClock:
            view.title = key("clock");
            setSlot(view, 1U, "language",
                    FermentationUiWorkspaceSlotAction::NavigateLanguage);
            setSlot(view, 2U, "network",
                    FermentationUiWorkspaceSlotAction::NavigateNetwork);
            break;
        case FermentationUiPage::Home:
            break;
    }
    return view;
}

FermentationUiWorkspaceView FermentationTouchWorkspace::view(
    const FermentationUiSnapshot& snapshot,
    const ProgramCatalog* catalog) const {
    return makePageView(snapshot, catalog);
}

void FermentationTouchWorkspace::setManualHoldingValues(
    FermentationUiManualRunPlanValues values) noexcept {
    markRenderRelevantChange();
    manualHoldingValues_ = std::move(values);
}

void FermentationTouchWorkspace::setManualTimedValues(
    ManualTimedRunValues values) noexcept {
    markRenderRelevantChange();
    manualTimedValues_ = std::move(values);
}

void FermentationTouchWorkspace::setCompletionCoolingPlan(
    std::optional<FermentationUiManualRunPlanValues> values) noexcept {
    markRenderRelevantChange();
    completionCoolingPlan_ = std::move(values);
}

void FermentationTouchWorkspace::setStopCoolingPlan(
    std::optional<FermentationUiManualRunPlanValues> values) noexcept {
    markRenderRelevantChange();
    stopCoolingPlan_ = std::move(values);
}

void FermentationTouchWorkspace::setSelectedMessage(
    std::optional<std::uint32_t> messageId) noexcept {
    markRenderRelevantChange();
    selectedMessageId_ = messageId;
}

void FermentationTouchWorkspace::setProgramEditCandidate(
    std::optional<ProgramDocument> candidate) noexcept {
    markRenderRelevantChange();
    programEditCandidate_ = std::move(candidate);
    programEditDirty_ = programEditCandidate_.has_value();
}

void FermentationTouchWorkspace::setProgramEditOperation(
    FermentationUiProgramEditOperation operation) noexcept {
    markRenderRelevantChange();
    programEditOperation_ = operation;
}

void FermentationTouchWorkspace::setSensorSelectionAction(
    std::optional<SensorSelectionUserAction> action) noexcept {
    markRenderRelevantChange();
    sensorSelectionAction_ = action;
}

void FermentationTouchWorkspace::setRecoveryTimeCorrectionSeconds(
    std::optional<std::uint32_t> seconds) noexcept {
    markRenderRelevantChange();
    recoveryTimeCorrectionSeconds_ = seconds;
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
    return selectProgramFor(programId, catalog,
                            FermentationUiPage::ProgramSummary);
}

// Every installed, listed program is selectable (administration must reach
// programs that cannot be started). Only a startable program becomes the
// start candidate; for the others `confirm` stays disabled and the page shows
// the blocked reason of the list projection.
bool FermentationTouchWorkspace::selectProgramFor(
    const std::string& programId, const ProgramCatalog& catalog,
    FermentationUiPage destination) {
    markRenderRelevantChange();
    const auto entries = makeFermentationUiProgramList(catalog);
    const auto found =
        std::find_if(entries.begin(), entries.end(),
                     [&programId](const FermentationUiProgramListEntry& entry) {
                         return entry.program.program.id == programId;
                     });
    if (found == entries.end()) return false;
    selectedProgramId_ = programId;
    selectedCandidate_ = {};
    if (found->startable) selectedCandidate_.programId = programId;
    programEditOperation_ = FermentationUiProgramEditOperation::Edit;
    programEditCandidate_.reset();
    setCanonicalPageStack(destination);
    return true;
}

void FermentationTouchWorkspace::setStartCandidate(
    FermentationUiStartCandidate candidate) {
    markRenderRelevantChange();
    if (!selectedProgramId_.has_value() ||
        candidate.programId != *selectedProgramId_) {
        selectedCandidate_ = {};
        return;
    }
    selectedCandidate_ = std::move(candidate);
}

bool FermentationTouchWorkspace::navigate(
    FermentationUiWorkspaceSlotAction action) {
    FermentationUiPage destination = page_;
    switch (action) {
        case FermentationUiWorkspaceSlotAction::NavigateHome:
            setCanonicalPageStack(FermentationUiPage::Home);
            return true;
        case FermentationUiWorkspaceSlotAction::NavigateBack:
            return goBack();
        case FermentationUiWorkspaceSlotAction::NavigateProgramList:
            programListIntent_ = FermentationUiProgramListIntent::Start;
            destination = FermentationUiPage::ProgramList;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateProgramManagement:
            programListIntent_ = FermentationUiProgramListIntent::Manage;
            destination = FermentationUiPage::ProgramList;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateProgramSummary:
            destination = FermentationUiPage::ProgramSummary;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateProgramEdit:
            destination = FermentationUiPage::ProgramEdit;
            break;
        case FermentationUiWorkspaceSlotAction::
            NavigateProgramDeleteConfirmation:
            destination = FermentationUiPage::ProgramDeleteConfirmation;
            break;
        case FermentationUiWorkspaceSlotAction::
            NavigateProgramDeleteFinalConfirmation:
            destination = FermentationUiPage::ProgramDeleteFinalConfirmation;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateProgramActions:
            destination = FermentationUiPage::ProgramActions;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateManualModeSelection:
            destination = FermentationUiPage::ManualModeSelection;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateManualHolding:
            destination = FermentationUiPage::ManualHolding;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateManualTimed:
            destination = FermentationUiPage::ManualTimed;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateProcess:
            destination = FermentationUiPage::Process;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateStopDialog:
            destination = FermentationUiPage::StopDialog;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateTechnical:
            destination = FermentationUiPage::Technical;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateMessages:
            destination = FermentationUiPage::Messages;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateMessageDetail:
            destination = FermentationUiPage::MessageDetail;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateCompletion:
            destination = FermentationUiPage::Completion;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateStatus:
            destination = FermentationUiPage::Status;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateDiagnostics:
            destination = FermentationUiPage::Diagnostics;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateService:
            destination = FermentationUiPage::Service;
            break;
        case FermentationUiWorkspaceSlotAction::NavigatePin:
            destination = FermentationUiPage::Pin;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateRecovery:
            destination = FermentationUiPage::Recovery;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateLanguage:
            destination = FermentationUiPage::HeaderLanguage;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateNetwork:
            destination = FermentationUiPage::HeaderNetwork;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateClock:
            destination = FermentationUiPage::HeaderClock;
            break;
        case FermentationUiWorkspaceSlotAction::NavigateWebAccess:
            destination = FermentationUiPage::HeaderWebAccess;
            break;
        case FermentationUiWorkspaceSlotAction::BeginProgramEdit:
        case FermentationUiWorkspaceSlotAction::CopyProgram:
        case FermentationUiWorkspaceSlotAction::NewProgram:
            destination = FermentationUiPage::ProgramEdit;
            break;
        default:
            return false;
    }
    if (destination == page_) return false;
    pageStack_.push_back(destination);
    page_ = destination;
    pager_.currentIndex = 0U;
    return true;
}

bool FermentationTouchWorkspace::goBack() {
    if (page_ == FermentationUiPage::Home || pageStack_.size() <= 1U)
        return false;
    if (pageStack_.size() == 2U) {
        pageStack_.resize(1U);
        page_ = FermentationUiPage::Home;
    } else {
        pageStack_.pop_back();
        page_ = pageStack_.back();
    }
    programEditDirty_ = false;
    return true;
}

FermentationUiWorkspacePress FermentationTouchWorkspace::pressSlot(
    const FermentationUiSnapshot& snapshot, std::size_t slotIndex,
    const FermentationUiWorkspaceView& current) {
    FermentationUiWorkspacePress result;
    if (slotIndex >= current.slotActions.size()) return result;
    const auto action = current.slotActions[slotIndex];
    switch (action) {
        case FermentationUiWorkspaceSlotAction::BeginProgramEdit:
            programEditOperation_ = FermentationUiProgramEditOperation::Edit;
            result.navigated = navigate(action);
            break;
        case FermentationUiWorkspaceSlotAction::CopyProgram:
            programEditOperation_ = FermentationUiProgramEditOperation::Copy;
            programEditCandidate_.reset();
            result.navigated = navigate(action);
            break;
        case FermentationUiWorkspaceSlotAction::NewProgram:
            selectedProgramId_.reset();
            selectedCandidate_ = {};
            programEditOperation_ = FermentationUiProgramEditOperation::New;
            programEditCandidate_.reset();
            result.navigated = navigate(action);
            break;
        case FermentationUiWorkspaceSlotAction::ResetProgram:
            if (selectedProgramId_.has_value()) {
                result.programEdit = FermentationUiProgramEditRequest{
                    FermentationUiProgramEditOperation::Reset,
                    *selectedProgramId_, std::nullopt, std::nullopt, true};
            }
            break;
        case FermentationUiWorkspaceSlotAction::UninstallProgram:
            if (selectedProgramId_.has_value()) {
                if (page_ == FermentationUiPage::ProgramEdit) {
                    programEditOperation_ =
                        FermentationUiProgramEditOperation::Uninstall;
                    result.navigated =
                        navigate(FermentationUiWorkspaceSlotAction::
                                     NavigateProgramDeleteConfirmation);
                } else if (page_ ==
                           FermentationUiPage::ProgramDeleteFinalConfirmation) {
                    result.programEdit = FermentationUiProgramEditRequest{
                        FermentationUiProgramEditOperation::Uninstall,
                        *selectedProgramId_, std::nullopt, std::nullopt, true};
                }
            }
            break;
        case FermentationUiWorkspaceSlotAction::DeleteProgram:
            if (selectedProgramId_.has_value()) {
                if (page_ == FermentationUiPage::ProgramEdit) {
                    programEditOperation_ =
                        FermentationUiProgramEditOperation::Delete;
                    result.navigated =
                        navigate(FermentationUiWorkspaceSlotAction::
                                     NavigateProgramDeleteConfirmation);
                } else if (page_ ==
                           FermentationUiPage::ProgramDeleteFinalConfirmation) {
                    result.programEdit = FermentationUiProgramEditRequest{
                        FermentationUiProgramEditOperation::Delete,
                        *selectedProgramId_, std::nullopt, std::nullopt, true};
                }
            }
            break;
        case FermentationUiWorkspaceSlotAction::SaveProgram:
            if (programEditOperation_ !=
                    FermentationUiProgramEditOperation::Edit ||
                programEditCandidate_.has_value()) {
                result.programEdit = FermentationUiProgramEditRequest{
                    programEditOperation_, selectedProgramId_.value_or(""),
                    programEditCandidate_, std::nullopt, true};
                programEditDirty_ = false;
            }
            break;
        case FermentationUiWorkspaceSlotAction::ApplySensorSelection:
            if (sensorSelectionAction_.has_value()) {
                result.action = FermentationUiEnvelopePayload{
                    FermentationUiSensorSelectionIntent{
                        *sensorSelectionAction_}};
            }
            break;
        case FermentationUiWorkspaceSlotAction::ApplyRecoveryTimeCorrection:
            if (recoveryTimeCorrectionSeconds_.has_value()) {
                result.action = FermentationUiEnvelopePayload{
                    FermentationUiRecoveryTimeCorrectionIntent{
                        *recoveryTimeCorrectionSeconds_}};
            }
            break;
        case FermentationUiWorkspaceSlotAction::ApplyNetworkModeApOnly:
            result.applyNetworkMode = FermentationUiApplyNetworkModeCommand{
                device_platform::NetworkMode::AP_ONLY};
            break;
        case FermentationUiWorkspaceSlotAction::ApplyNetworkModeHomeWifi:
            result.applyNetworkMode = FermentationUiApplyNetworkModeCommand{
                device_platform::NetworkMode::HOME_WIFI};
            break;
        case FermentationUiWorkspaceSlotAction::BeginHomeWifiReconfiguration:
            result.beginHomeWifiReconfiguration =
                FermentationUiBeginHomeWifiReconfigurationCommand{};
            break;
        case FermentationUiWorkspaceSlotAction::OpenWebProvisioningWindow:
            result.openWebProvisioningWindow =
                FermentationUiOpenWebProvisioningWindowCommand{};
            break;
        case FermentationUiWorkspaceSlotAction::MovePagerUp:
            result.navigated = pager_.moveUp();
            break;
        case FermentationUiWorkspaceSlotAction::MovePagerDown:
            result.navigated = pager_.moveDown();
            break;
        case FermentationUiWorkspaceSlotAction::NavigateMessageDetail:
            if (!snapshot.messages.empty()) {
                const auto decision = std::find_if(
                    snapshot.messages.begin(), snapshot.messages.end(),
                    [](const MessageView& message) {
                        return message.message.active &&
                               !message.message.resolved;
                    });
                if (decision != snapshot.messages.end())
                    selectedMessageId_ = decision->message.id;
            }
            result.navigated = navigate(action);
            break;
        case FermentationUiWorkspaceSlotAction::ProductInsertedConfirmed:
            if (snapshot.home.mode == FermentationHomeMode::Waiting &&
                snapshot.home.processState == ProcessState::WaitingForProduct) {
                result.transitionAction =
                    FermentationUiProductInsertedConfirmedIntent{};
            }
            break;
        case FermentationUiWorkspaceSlotAction::StartProgram:
            if (page_ == FermentationUiPage::ProgramSummary &&
                selectedProgramId_.has_value() &&
                selectedCandidate_.programId == *selectedProgramId_) {
                result.action = FermentationUiEnvelopePayload{
                    FermentationUiStartProgramIntent{selectedCandidate_}};
            }
            break;
        case FermentationUiWorkspaceSlotAction::StartManualHolding:
            if (manualHoldingValues_.has_value())
                result.action = FermentationUiEnvelopePayload{
                    makeManualHoldingIntent(*manualHoldingValues_)};
            break;
        case FermentationUiWorkspaceSlotAction::StartManualTimed:
            if (manualTimedValues_.has_value())
                result.action = FermentationUiEnvelopePayload{
                    makeManualTimedIntent(*manualTimedValues_)};
            break;
        case FermentationUiWorkspaceSlotAction::StopTurnOff:
            result.action = FermentationUiEnvelopePayload{
                makeStopIntent(StopOption::AbortAndTurnOff)};
            break;
        case FermentationUiWorkspaceSlotAction::StopAndCool:
            if (stopCoolingPlan_.has_value())
                result.action = FermentationUiEnvelopePayload{
                    makeStopIntent(StopOption::AbortAndCool, stopCoolingPlan_)};
            break;
        case FermentationUiWorkspaceSlotAction::Complete:
            result.action =
                FermentationUiEnvelopePayload{makeCompletionIntent(false)};
            break;
        case FermentationUiWorkspaceSlotAction::CompleteAndCool:
            if (completionCoolingPlan_.has_value())
                result.action = FermentationUiEnvelopePayload{
                    makeCompletionIntent(true, completionCoolingPlan_)};
            break;
        case FermentationUiWorkspaceSlotAction::ResumeFallback:
            result.resumeFallback =
                FermentationUiResumeFallbackCommand{snapshot.revisions, false};
            break;
        case FermentationUiWorkspaceSlotAction::AcknowledgeMessage:
            if (selectedMessageId_.has_value())
                result.action = FermentationUiEnvelopePayload{
                    FermentationUiAcknowledgeMessageIntent{
                        *selectedMessageId_}};
            break;
        case FermentationUiWorkspaceSlotAction::MuteMessage:
            if (selectedMessageId_.has_value())
                result.action = FermentationUiEnvelopePayload{
                    FermentationUiMuteMessageIntent{*selectedMessageId_}};
            break;
        case FermentationUiWorkspaceSlotAction::ResetFault:
            result.action =
                FermentationUiEnvelopePayload{FermentationUiResetFaultIntent{}};
            break;
        case FermentationUiWorkspaceSlotAction::NavigateBack:
            result.navigated = goBack();
            break;
        default:
            result.navigated = navigate(action);
            break;
    }
    return result;
}

FermentationUiWorkspacePress FermentationTouchWorkspace::press(
    const FermentationUiSnapshot& snapshot,
    const device_platform::DeviceUiTarget& target,
    const ProgramCatalog* catalog) {
    markRenderRelevantChange();
    const auto current = view(snapshot, catalog);
    pager_.itemCount = current.pager.itemCount;
    pager_.currentIndex = current.pager.currentIndex;
    FermentationUiWorkspacePress result;
    bool enabled = target.valid();
    device_platform::DeviceUiTarget selectionTarget = target;
    if (target.kind == device_platform::DeviceUiTargetKind::BottomSlot) {
        if (target.slotIndex >= current.bottomSlots.size()) {
            enabled = false;
        } else {
            enabled = current.bottomSlots[target.slotIndex].enabled &&
                      current.slotActions[target.slotIndex] !=
                          FermentationUiWorkspaceSlotAction::None;
            if (isPageExitAction(current.slotActions[target.slotIndex]))
                selectionTarget.kind =
                    device_platform::DeviceUiTargetKind::HomeOrBack;
        }
    } else if (target.kind ==
               device_platform::DeviceUiTargetKind::ContentCell) {
        // Only the visible window of the program list is hittable: row r is
        // entry currentIndex + r.
        enabled = page_ == FermentationUiPage::ProgramList &&
                  target.column == 0U &&
                  target.row < kFermentationUiListVisibleRows &&
                  current.pager.currentIndex + target.row <
                      current.programList.size();
    } else if (target.kind == device_platform::DeviceUiTargetKind::PagerUp) {
        enabled = current.pager.canMoveUp();
    } else if (target.kind == device_platform::DeviceUiTargetKind::PagerDown) {
        enabled = current.pager.canMoveDown();
    } else if (target.kind == device_platform::DeviceUiTargetKind::HomeOrBack ||
               target.kind == device_platform::DeviceUiTargetKind::Back ||
               target.kind == device_platform::DeviceUiTargetKind::Cancel) {
        enabled = page_ != FermentationUiPage::Home;
    } else if (target.kind ==
                   device_platform::DeviceUiTargetKind::HeaderLanguage ||
               target.kind ==
                   device_platform::DeviceUiTargetKind::HeaderNetwork ||
               target.kind ==
                   device_platform::DeviceUiTargetKind::HeaderClock) {
        selectionTarget.kind = device_platform::DeviceUiTargetKind::HomeOrBack;
    }

    result.interaction = device_platform::selectDeviceUiTarget(
        {selectionTarget, enabled, false, false,
         current.route.exitRequirement});
    if (result.interaction.outcome !=
        device_platform::DeviceUiInteractionOutcome::TargetSelected) {
        return result;
    }

    switch (target.kind) {
        case device_platform::DeviceUiTargetKind::BottomSlot:
            return [&]() {
                auto pressed = pressSlot(snapshot, target.slotIndex, current);
                pressed.interaction = result.interaction;
                return pressed;
            }();
        case device_platform::DeviceUiTargetKind::ContentCell: {
            // `enabled` guarantees ProgramList, a catalog behind the list and
            // an in-range entry.
            const auto& entry =
                current.programList[current.pager.currentIndex + target.row];
            result.navigated = selectProgramFor(
                entry.program.program.id, *catalog,
                programListIntent_ == FermentationUiProgramListIntent::Manage
                    ? FermentationUiPage::ProgramActions
                    : FermentationUiPage::ProgramSummary);
            return result;
        }
        case device_platform::DeviceUiTargetKind::PagerUp:
            result.navigated = pager_.moveUp();
            return result;
        case device_platform::DeviceUiTargetKind::PagerDown:
            result.navigated = pager_.moveDown();
            return result;
        case device_platform::DeviceUiTargetKind::HomeOrBack:
        case device_platform::DeviceUiTargetKind::Back:
        case device_platform::DeviceUiTargetKind::Cancel:
            result.navigated = goBack();
            return result;
        case device_platform::DeviceUiTargetKind::HeaderLanguage:
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateLanguage);
            return result;
        case device_platform::DeviceUiTargetKind::HeaderNetwork:
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateNetwork);
            return result;
        case device_platform::DeviceUiTargetKind::HeaderClock:
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateClock);
            return result;
        case device_platform::DeviceUiTargetKind::Confirm: {
            std::optional<std::size_t> confirmSlot;
            switch (page_) {
                case FermentationUiPage::StopDialog:
                    confirmSlot = 1U;
                    break;
                case FermentationUiPage::ProgramSummary:
                case FermentationUiPage::ManualHolding:
                case FermentationUiPage::ManualTimed:
                case FermentationUiPage::Completion:
                case FermentationUiPage::Recovery:
                    confirmSlot = 2U;
                    break;
                case FermentationUiPage::ProgramEdit:
                    confirmSlot = 3U;
                    break;
                case FermentationUiPage::ProgramDeleteConfirmation:
                    confirmSlot = 1U;
                    break;
                case FermentationUiPage::ProgramDeleteFinalConfirmation:
                    confirmSlot = 3U;
                    break;
                default:
                    break;
            }
            if (!confirmSlot.has_value() ||
                !current.bottomSlots[*confirmSlot].enabled ||
                current.slotActions[*confirmSlot] ==
                    FermentationUiWorkspaceSlotAction::None) {
                result.interaction.outcome =
                    device_platform::DeviceUiInteractionOutcome::Blocked;
                result.interaction.feedback =
                    device_platform::DeviceUiFeedbackIntent::ActionRejected;
                result.interaction.visiblePressFeedback = true;
                return result;
            }
            const auto pressed = pressSlot(snapshot, *confirmSlot, current);
            result.navigated = pressed.navigated;
            result.action = pressed.action;
            result.transitionAction = pressed.transitionAction;
            result.resumeFallback = pressed.resumeFallback;
            result.programEdit = pressed.programEdit;
            result.applyNetworkMode = pressed.applyNetworkMode;
            result.beginHomeWifiReconfiguration =
                pressed.beginHomeWifiReconfiguration;
            result.openWebProvisioningWindow =
                pressed.openWebProvisioningWindow;
            return result;
        }
        case device_platform::DeviceUiTargetKind::None:
            return result;
    }
    return result;
}

void FermentationTouchWorkspace::setPage(FermentationUiPage page) {
    markRenderRelevantChange();
    setCanonicalPageStack(page);
}

}  // namespace fermentation
