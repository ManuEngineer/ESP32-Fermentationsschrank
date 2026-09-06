#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "device_ui_interaction.hpp"
#include "fermentation_ui_commands.hpp"
#include "fermentation_ui_editing.hpp"
#include "fermentation_ui_models.hpp"

namespace fermentation {

enum class FermentationUiPage : std::uint8_t {
    Home,
    ProgramList,
    ProgramSummary,
    ProgramEdit,
    ProgramDeleteConfirmation,
    ProgramDeleteFinalConfirmation,
    ProgramActions,
    ManualModeSelection,
    ManualHolding,
    ManualTimed,
    Process,
    StopDialog,
    Technical,
    Messages,
    MessageDetail,
    Completion,
    Status,
    Diagnostics,
    Service,
    Pin,
    Recovery,
    HeaderLanguage,
    HeaderNetwork,
    HeaderClock,
};

enum class FermentationUiSafeBootTarget : std::uint8_t {
    PersistentFactoryReset,
    RawTouchRecovery,
    NetworkProvisioningRecovery,
    DiagnosticsExport,
    ResumeFallback,
};

enum class FermentationUiSafeBootCapability : std::uint8_t {
    PersistentFactoryReset,
    RawTouchRecovery,
    NetworkProvisioningRecovery,
    DiagnosticsExport,
    ExistingRecoveryPath,
};

[[nodiscard]] FermentationUiSafeBootCapability safeBootCapabilityFor(
    FermentationUiSafeBootTarget target) noexcept;

// Workspace-local routing: this is not a second command tag. An enabled slot
// either navigates or maps to exactly one existing intent.
enum class FermentationUiWorkspaceSlotAction : std::uint8_t {
    None,
    NavigateBack,
    NavigateHome,
    NavigateProgramList,
    NavigateProgramSummary,
    NavigateProgramEdit,
    NavigateProgramDeleteConfirmation,
    NavigateProgramDeleteFinalConfirmation,
    NavigateProgramActions,
    NavigateManualModeSelection,
    NavigateManualHolding,
    NavigateManualTimed,
    NavigateProcess,
    NavigateStopDialog,
    NavigateTechnical,
    NavigateMessages,
    NavigateMessageDetail,
    NavigateCompletion,
    NavigateStatus,
    NavigateDiagnostics,
    NavigateService,
    NavigatePin,
    NavigateRecovery,
    NavigateLanguage,
    NavigateNetwork,
    NavigateClock,
    MovePagerUp,
    MovePagerDown,
    BeginProgramEdit,
    CopyProgram,
    NewProgram,
    ResetProgram,
    UninstallProgram,
    DeleteProgram,
    SaveProgram,
    ApplySensorSelection,
    ApplyRecoveryTimeCorrection,
    StartProgram,
    StartManualHolding,
    StartManualTimed,
    ProductInsertedConfirmed,
    StopTurnOff,
    StopAndCool,
    Complete,
    CompleteAndCool,
    ResumeFallback,
    AcknowledgeMessage,
    MuteMessage,
    ResetFault,
};

struct FermentationUiWorkspaceView {
    FermentationUiPage page{FermentationUiPage::Home};
    device_platform::TextKey title;
    device_platform::ShellRoute route;
    std::array<device_platform::BottomSlot, 4U> bottomSlots{};
    std::array<FermentationUiWorkspaceSlotAction, 4U> slotActions{};
    std::vector<FermentationUiProgramListEntry> programList;
    std::optional<std::string> confirmationProgramName;
    device_platform::VerticalPager pager;
    // view() has no implicit command. A command is returned only by press()
    // for the explicitly selected action slot.
    std::optional<FermentationUiEnvelopePayload> action;
    std::optional<FermentationUiProductInsertedConfirmedIntent>
        transitionAction;
    std::optional<device_platform::TextKey> blockedReason;
    std::vector<FermentationUiSafeBootCapability> unavailableCapabilities;
    bool completionLocked{false};
};

struct FermentationUiWorkspacePress {
    device_platform::DeviceUiInteractionResult interaction;
    bool navigated{false};
    std::optional<FermentationUiEnvelopePayload> action;
    std::optional<FermentationUiProductInsertedConfirmedIntent>
        transitionAction;
    std::optional<FermentationUiResumeFallbackCommand> resumeFallback;
    std::optional<FermentationUiProgramEditRequest> programEdit;
};

class FermentationTouchWorkspace {
   public:
    [[nodiscard]] FermentationUiWorkspaceView view(
        const FermentationUiSnapshot& snapshot,
        const ProgramCatalog* catalog = nullptr) const;

    [[nodiscard]] FermentationUiWorkspacePress press(
        const FermentationUiSnapshot& snapshot,
        const device_platform::DeviceUiTarget& target,
        const ProgramCatalog* catalog = nullptr);

    [[nodiscard]] bool selectProgram(const std::string& programId,
                                     const ProgramCatalog& catalog);
    // The candidate is the one canonical start payload. A candidate for a
    // different selected program is rejected instead of being silently
    // substituted at press time.
    void setStartCandidate(FermentationUiStartCandidate candidate);
    void setManualHoldingValues(
        FermentationUiManualRunPlanValues values) noexcept;
    void setManualTimedValues(ManualTimedRunValues values) noexcept;
    void setCompletionCoolingPlan(
        std::optional<FermentationUiManualRunPlanValues> values) noexcept;
    void setStopCoolingPlan(
        std::optional<FermentationUiManualRunPlanValues> values) noexcept;
    void setSelectedMessage(std::optional<std::uint32_t> messageId) noexcept;
    void setProgramEditCandidate(
        std::optional<ProgramDocument> candidate) noexcept;
    void setProgramEditOperation(
        FermentationUiProgramEditOperation operation) noexcept;
    void setSensorSelectionAction(
        std::optional<SensorSelectionUserAction> action) noexcept;
    void setRecoveryTimeCorrectionSeconds(
        std::optional<std::uint32_t> seconds) noexcept;
    void setProgramEditDirty(bool dirty) noexcept { programEditDirty_ = dirty; }

    [[nodiscard]] FermentationUiStartManualHoldingIntent
    makeManualHoldingIntent(
        const FermentationUiManualRunPlanValues& values) const;
    [[nodiscard]] FermentationUiStartManualTimedIntent makeManualTimedIntent(
        const ManualTimedRunValues& values) const;
    [[nodiscard]] FermentationUiStopRunIntent makeStopIntent(
        StopOption option,
        const std::optional<FermentationUiManualRunPlanValues>& coolingPlan =
            std::nullopt) const;
    [[nodiscard]] FermentationUiCompleteRunIntent makeCompletionIntent(
        bool startCooling,
        const std::optional<FermentationUiManualRunPlanValues>& coolingPlan =
            std::nullopt) const;
    [[nodiscard]] bool movePagerUp() noexcept { return pager_.moveUp(); }
    [[nodiscard]] bool movePagerDown() noexcept { return pager_.moveDown(); }
    [[nodiscard]] FermentationUiPage page() const noexcept { return page_; }
    void setPage(FermentationUiPage page);
    [[nodiscard]] const std::optional<std::string>& selectedProgramId()
        const noexcept {
        return selectedProgramId_;
    }

   private:
    [[nodiscard]] static device_platform::TextKey key(const char* value);
    [[nodiscard]] static device_platform::BottomSlot slot(const char* label,
                                                          bool enabled = true);
    [[nodiscard]] static bool isPageExitAction(
        FermentationUiWorkspaceSlotAction action) noexcept;
    [[nodiscard]] static std::vector<device_platform::TextKey> routeForPage(
        FermentationUiPage page);
    [[nodiscard]] FermentationUiWorkspaceView makeHomeView(
        const FermentationUiSnapshot& snapshot) const;
    [[nodiscard]] FermentationUiWorkspaceView makePageView(
        const FermentationUiSnapshot& snapshot,
        const ProgramCatalog* catalog) const;
    [[nodiscard]] FermentationUiWorkspacePress pressSlot(
        const FermentationUiSnapshot& snapshot, std::size_t slotIndex,
        const FermentationUiWorkspaceView& current);
    [[nodiscard]] bool navigate(FermentationUiWorkspaceSlotAction action);
    [[nodiscard]] bool goBack();
    void setSlot(FermentationUiWorkspaceView& view, std::size_t index,
                 const char* label, FermentationUiWorkspaceSlotAction action,
                 bool enabled = true) const;
    void setCanonicalPageStack(FermentationUiPage page);

    FermentationUiPage page_{FermentationUiPage::Home};
    device_platform::VerticalPager pager_;
    std::vector<FermentationUiPage> pageStack_{FermentationUiPage::Home};
    std::optional<std::string> selectedProgramId_;
    FermentationUiStartCandidate selectedCandidate_;
    std::optional<FermentationUiManualRunPlanValues> manualHoldingValues_;
    std::optional<ManualTimedRunValues> manualTimedValues_;
    std::optional<FermentationUiManualRunPlanValues> completionCoolingPlan_;
    std::optional<FermentationUiManualRunPlanValues> stopCoolingPlan_;
    std::optional<std::uint32_t> selectedMessageId_;
    std::optional<ProgramDocument> programEditCandidate_;
    FermentationUiProgramEditOperation programEditOperation_{
        FermentationUiProgramEditOperation::Edit};
    std::optional<SensorSelectionUserAction> sensorSelectionAction_;
    std::optional<std::uint32_t> recoveryTimeCorrectionSeconds_;
    bool programEditDirty_{false};
};

}  // namespace fermentation
