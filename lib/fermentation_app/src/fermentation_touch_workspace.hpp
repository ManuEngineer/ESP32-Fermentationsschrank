#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "device_ui_interaction.hpp"
#include "fermentation_ui_commands.hpp"
#include "fermentation_ui_models.hpp"

namespace fermentation {

enum class FermentationUiPage : std::uint8_t {
    Home,
    ProgramList,
    ProgramSummary,
    ProgramEdit,
    Process,
    Technical,
    Messages,
    MessageDetail,
    Completion,
    Status,
    Diagnostics,
    Service,
    Pin,
    Recovery,
};

enum class FermentationUiSafeBootTarget : std::uint8_t {
    PersistentFactoryReset,
    RawTouchRecovery,
    NetworkProvisioningRecovery,
    DiagnosticsExport,
    ResumeFallback,
};

enum class FermentationUiSafeBootOwner : std::uint8_t {
    Issue57,
    Issue31,
    Issue89,
    Issue28,
    ExistingRecoveryPath,
};

[[nodiscard]] FermentationUiSafeBootOwner safeBootOwnerFor(
    FermentationUiSafeBootTarget target) noexcept;

struct FermentationUiWorkspaceView {
    FermentationUiPage page{FermentationUiPage::Home};
    device_platform::TextKey title;
    device_platform::ShellRoute route;
    std::array<device_platform::BottomSlot, 4U> bottomSlots{};
    device_platform::VerticalPager pager;
    std::optional<FermentationUiEnvelopePayload> action;
    std::optional<FermentationUiProductInsertedConfirmedIntent>
        transitionAction;
    std::optional<device_platform::TextKey> blockedReason;
    std::optional<FermentationUiSafeBootOwner> unavailableOwner;
    bool completionLocked{false};
};

struct FermentationUiWorkspacePress {
    device_platform::DeviceUiInteractionResult interaction;
    bool navigated{false};
    std::optional<FermentationUiEnvelopePayload> action;
    std::optional<FermentationUiProductInsertedConfirmedIntent>
        transitionAction;
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
    void setPage(FermentationUiPage page) noexcept { page_ = page; }
    [[nodiscard]] const std::optional<std::string>& selectedProgramId()
        const noexcept {
        return selectedProgramId_;
    }

   private:
    [[nodiscard]] static device_platform::TextKey key(const char* value);
    [[nodiscard]] static device_platform::BottomSlot slot(const char* label,
                                                          bool enabled = true);
    [[nodiscard]] FermentationUiWorkspaceView makeHomeView(
        const FermentationUiSnapshot& snapshot) const;
    [[nodiscard]] FermentationUiWorkspaceView makePageView(
        const FermentationUiSnapshot& snapshot,
        const ProgramCatalog* catalog) const;
    [[nodiscard]] std::optional<FermentationUiEnvelopePayload> homeAction(
        const FermentationUiSnapshot& snapshot) const;

    FermentationUiPage page_{FermentationUiPage::Home};
    device_platform::VerticalPager pager_;
    std::optional<std::string> selectedProgramId_;
};

}  // namespace fermentation
