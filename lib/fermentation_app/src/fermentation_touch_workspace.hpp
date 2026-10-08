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
    HeaderWebAccess,
    ValueEdit,
    Settings,
    TextEdit,
    FactoryReset,
};

// Rows of the normal settings page in the order the owner decided (O1).
enum class FermentationUiSettingsRow : std::uint8_t {
    Language,
    TimeZone,
    DeviceName,
    Network,
    WebAccess,
    Service,
};
inline constexpr std::size_t kFermentationUiSettingsRowCount = 6U;

// Text the shared on-screen keyboard edits (O3).
enum class FermentationUiTextTarget : std::uint8_t {
    DeviceName,
    ProgramName,
    ProgramNotes,
};

// Keyboard grid (S10, O3): 4 rows x 10 columns. Rows 0-2 carry the characters
// of the current mode; row 3 is `Clear` (columns 0-1), `Space` (2-7), `-` (8)
// and `.` (9).
inline constexpr std::uint8_t kFermentationUiKeyboardRows = 4U;
inline constexpr std::uint8_t kFermentationUiKeyboardColumns = 10U;

enum class FermentationUiKeyboardKeyKind : std::uint8_t {
    None,
    Character,
    Clear,
};

struct FermentationUiKeyboardKey {
    FermentationUiKeyboardKeyKind kind{FermentationUiKeyboardKeyKind::None};
    char character{'\0'};
};

// The key at a grid cell for the keyboard mode; the single layout definition
// for the workspace (hit routing) and the renderer (drawing). Only ASCII is
// offered.
[[nodiscard]] FermentationUiKeyboardKey fermentationUiKeyboardKeyAt(
    TextEditMode mode, std::uint8_t row, std::uint8_t column) noexcept;

// Program fields of the local editor (S10, D9): the fields
// LOCAL_UI_PROGRAMS.md names, as far as the program model has them. The
// technical qualification values stay out (service area).
enum class FermentationUiProgramField : std::uint8_t {
    Name,
    Notes,
    TargetTemperature,
    Duration,
    Preheat,
    MaxProductWait,
    SensorPreference,
    FailurePolicy,
    FallbackDelay,
    ReturnStrategy,
    MaxTargetReach,
    CompletionMode,
    CoolingTarget,
    HoldDuration,
};
inline constexpr std::size_t kFermentationUiProgramFieldCount = 14U;

// Next-run start values editable on ProgramSummary (S8). One explicit enum,
// no generic form model.
enum class FermentationUiStartField : std::uint8_t {
    TargetTemperature,
    Duration,
    Preheat,
    SensorMode,
    CompletionMode,
    CoolingTarget,
    HoldDuration,
};
inline constexpr std::size_t kFermentationUiStartFieldCount = 7U;

[[nodiscard]] inline bool isNumericStartField(
    FermentationUiStartField field) noexcept {
    return field == FermentationUiStartField::TargetTemperature ||
           field == FermentationUiStartField::Duration ||
           field == FermentationUiStartField::CoolingTarget ||
           field == FermentationUiStartField::HoldDuration;
}

// Duration-like numeric fields take whole minutes only.
[[nodiscard]] inline bool isWholeNumberStartField(
    FermentationUiStartField field) noexcept {
    return field == FermentationUiStartField::Duration ||
           field == FermentationUiStartField::HoldDuration;
}

// Keypad grid of the ValueEdit page (D8): 4 rows x 3 columns,
// `1 2 3 / 4 5 6 / 7 8 9 / . 0 +-`.
inline constexpr std::uint8_t kFermentationUiKeypadRows = 4U;
inline constexpr std::uint8_t kFermentationUiKeypadColumns = 3U;

// Intent with which the program list was opened: `Start` picks a program for a
// new run (ProgramSummary), `Manage` picks a program for administration
// (ProgramActions).
enum class FermentationUiProgramListIntent : std::uint8_t {
    Start,
    Manage,
};

// Visible content rows of a list page. The platform target only carries row
// indices; the capacity is owned here and by the renderer.
inline constexpr std::size_t kFermentationUiListVisibleRows = 3U;

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
    NavigateProgramManagement,
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
    ApplyNetworkModeApOnly,
    ApplyNetworkModeHomeWifi,
    BeginHomeWifiReconfiguration,
    NavigateWebAccess,
    OpenWebProvisioningWindow,
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
    ValueEditCancel,
    ValueEditBackspace,
    ValueEditClear,
    ValueEditCommit,
    ResetStartValues,
    NavigateSettings,
    TextEditCancel,
    TextEditMode,
    TextEditBackspace,
    TextEditCommit,
    DiscardProgramEdit,
    // Local factory reset flow (Issue #19). Begin also navigates to the
    // FactoryReset page; Hold carries no payload (sustained contact is
    // evaluated by the touch dispatcher, not by a press).
    FactoryResetBegin,
    FactoryResetAcknowledge,
    FactoryResetCancel,
    FactoryResetHold,
    FactoryResetDismiss,
};

// The bottom slot that is the hold target while the flow is in its hold stage.
inline constexpr std::size_t kFermentationUiFactoryResetHoldSlot = 1U;

// Secret-free content of the FactoryReset page; the renderer maps stage and
// outcome onto text.
struct FermentationUiFactoryResetPageView {
    FactoryResetStage stage{FactoryResetStage::Idle};
    FactoryResetOutcome outcome{FactoryResetOutcome::None};
    bool available{false};
    std::uint8_t holdProgressTenths{0U};
};

// Read-only content of `ProgramSummary` (S7): the selected program's values
// with the next-run candidate overrides already applied (override, else
// program value). It is a display projection only; the binding StartSummary
// stays the result of the command owner.
struct FermentationUiProgramSummaryView {
    std::string name;
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> durationMinutes;
    bool preheat{false};
    // A candidate override is a RunSensorMode; without one the program's own
    // SensorPreference is shown. The two enums are not mapped onto each other.
    SensorPreference sensorPreference{SensorPreference::AirProductOptional};
    std::optional<RunSensorMode> sensorModeOverride;
    CompletionMode completionMode{CompletionMode::FinishWithoutCooling};
    std::optional<double> coolingTargetCelsius;
    std::optional<std::uint32_t> holdDurationMinutes;
    // Editable fields in display order (cooling target / hold duration only
    // when the completion mode uses them) and which of them differ from the
    // stored program values (indexed by FermentationUiStartField).
    std::array<FermentationUiStartField, kFermentationUiStartFieldCount>
        fields{};
    std::size_t fieldCount{0U};
    std::array<bool, kFermentationUiStartFieldCount> changed{};
    // The program with the candidate overrides applied passes the existing
    // runnable validation; `confirm` stays disabled otherwise.
    bool valuesValid{false};
    // Fields are editable only for a startable program with a start candidate.
    bool editable{false};
    // Manual-run field list (S9): the page reuses this shape. `sensorMode` is
    // an explicit choice there (no stored preference), the technical limits
    // are never listed.
    bool manual{false};
    RunSensorMode sensorMode{RunSensorMode::Air};
    // First list row (rows above it hold other page content) and whether the
    // list needs its pager buttons (more fields than visible rows).
    std::uint8_t rowOffset{0U};
    bool pagerButtons{true};
};

// Where a field list writes: the next-run start candidate or the real run
// values entered for a manual run / cooling plan (S9).
enum class FermentationUiManualDraftSlot : std::uint8_t {
    StartCandidate,
    ManualHolding,
    ManualTimed,
    StopCooling,
    CompletionCooling,
};

inline constexpr std::size_t kFermentationUiManualDraftSlotCount = 5U;

// Real run values the user entered for a manual page; nothing is invented, so
// every value is optional until entered. Technical limits are not part of it.
struct FermentationUiManualDraft {
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> durationMinutes;
    std::optional<bool> preheat;
    std::optional<RunSensorMode> sensorMode;
    std::optional<CompletionMode> completionMode;
    std::optional<double> coolingTargetCelsius;
    std::optional<std::uint32_t> holdDurationMinutes;
};

enum class FermentationUiValueUnit : std::uint8_t {
    Celsius,
    Minutes,
    Seconds,
};

// Content of the shared numeric edit page.
struct FermentationUiValueEditView {
    FermentationUiStartField field{FermentationUiStartField::TargetTemperature};
    std::string candidate;
    bool commitValid{false};
    // Unit and key set of the field being edited (start values, manual values
    // and program fields share the page).
    FermentationUiValueUnit unit{FermentationUiValueUnit::Celsius};
    bool wholeNumber{false};
};

// Content of the normal settings page (S10). The device name is a read-only
// display copy of the owner's value.
struct FermentationUiSettingsView {
    std::string deviceName;
    // Display convenience only: the Application decides whether a run blocks
    // the change.
    bool deviceNameEditable{false};
    bool serviceAvailable{false};
    std::optional<device_platform::TextKey> serviceReason;
    bool deviceNameChangeFailed{false};
};

// Content of the shared on-screen keyboard page (S10, O3).
struct FermentationUiTextEditView {
    FermentationUiTextTarget target{FermentationUiTextTarget::DeviceName};
    std::string candidate;
    TextEditMode mode{TextEditMode::Lowercase};
    // The candidate passes the owning text rule (visible name / notes).
    bool commitValid{false};
    // No further character fits the owning byte limit.
    bool full{false};
};

// One row of the program editor: a label, and either a localized enum value
// or a formatted value text.
struct FermentationUiProgramEditRow {
    FermentationUiProgramField field{FermentationUiProgramField::Name};
    device_platform::TextKey label;
    std::string text;
    std::optional<device_platform::TextKey> valueKey;
    // A value that differs from the stored program is marked.
    bool changed{false};
};

// Content of the program editor for the Edit operation; Copy and New list the
// name only. Row validity is the existing program validator's verdict.
struct FermentationUiProgramEditView {
    // Heap-held (the workspace view is copied on small embedded stacks).
    std::vector<FermentationUiProgramEditRow> rows;
    std::size_t rowCount{0U};
    // The edited program passes the catalog-level program validation and the
    // owning text rules.
    bool valid{false};
};

struct FermentationUiWorkspaceView {
    FermentationUiPage page{FermentationUiPage::Home};
    device_platform::TextKey title;
    device_platform::ShellRoute route;
    std::array<device_platform::BottomSlot, 4U> bottomSlots{};
    std::array<FermentationUiWorkspaceSlotAction, 4U> slotActions{};
    std::vector<FermentationUiProgramListEntry> programList;
    std::optional<std::string> confirmationProgramName;
    std::optional<FermentationUiProgramSummaryView> programSummary;
    std::optional<FermentationUiValueEditView> valueEdit;
    std::optional<FermentationUiSettingsView> settings;
    std::optional<FermentationUiTextEditView> textEdit;
    std::optional<FermentationUiProgramEditView> programEdit;
    std::optional<FermentationUiFactoryResetPageView> factoryReset;
    std::optional<device_platform::TextKey> confirmationWarning;
    device_platform::VerticalPager pager;
    // view() has no implicit command. A command is returned only by press()
    // for the explicitly selected action slot.
    // Canonical id of the explicitly selected message (MessageDetail); the
    // renderer looks the message up in the snapshot, nothing is copied.
    std::optional<std::uint32_t> selectedMessageId;
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
    std::optional<FermentationUiApplyNetworkModeCommand> applyNetworkMode;
    std::optional<FermentationUiBeginHomeWifiReconfigurationCommand>
        beginHomeWifiReconfiguration;
    std::optional<FermentationUiOpenWebProvisioningWindowCommand>
        openWebProvisioningWindow;
    std::optional<FermentationUiSetDisplayLanguageCommand> setDisplayLanguage;
    std::optional<FermentationUiSetDeviceNameCommand> setDeviceName;
    std::optional<FermentationUiFactoryResetCommand> factoryReset;
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
    void setProgramEditDirty(bool dirty) noexcept {
        markRenderRelevantChange();
        programEditDirty_ = dirty;
    }

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
    [[nodiscard]] bool movePagerUp() noexcept {
        markRenderRelevantChange();
        return pager_.moveUp();
    }
    [[nodiscard]] bool movePagerDown() noexcept {
        markRenderRelevantChange();
        return pager_.moveDown();
    }
    [[nodiscard]] FermentationUiPage page() const noexcept { return page_; }
    // Read-only display copy of the owner's visible device name, handed in by
    // the render gate from the presentation source. Never edited locally;
    // changing it invalidates the render key like any visible input.
    void adoptDeviceName(const std::string& name) {
        if (deviceName_ == name) return;
        markRenderRelevantChange();
        deviceName_ = name;
    }
    // Records the owning outcome of a program edit request (save, reset,
    // delete): only an accepted request makes the editor clean and drops its
    // candidate; a refused one keeps both, so the user can retry or discard.
    void noteProgramEditOutcome(bool accepted) noexcept {
        markRenderRelevantChange();
        if (accepted) {
            programEditDirty_ = false;
            programEditCandidate_.reset();
            programEditName_.reset();
        }
    }
    // Records the owning outcome of the last device name commit so the
    // settings page can show a refused change (transient display state like
    // the language outcome).
    void noteDeviceNameOutcome(bool accepted) noexcept {
        markRenderRelevantChange();
        deviceNameChangeFailed_ = !accepted;
    }
    // Records the owning outcome of the last language row press so the
    // language page can show a failed change. Purely transient display state
    // (not a locale or configuration owner): the next outcome replaces it and
    // leaving the page discards it.
    void noteDisplayLanguageOutcome(bool accepted) noexcept {
        markRenderRelevantChange();
        displayLanguageChangeFailed_ = !accepted;
    }
    [[nodiscard]] FermentationUiProgramListIntent programListIntent()
        const noexcept {
        return programListIntent_;
    }
    void setPage(FermentationUiPage page);
    // Monotonic render-invalidation counter (wraps). It is increased by every
    // public mutator that can change what view() returns for an unchanged
    // snapshot and catalog, and serves only as a cache key for the renderer;
    // it is not application state.
    [[nodiscard]] std::uint32_t renderRevision() const noexcept {
        return renderRevision_;
    }
    [[nodiscard]] const std::optional<std::string>& selectedProgramId()
        const noexcept {
        return selectedProgramId_;
    }

   private:
    void markRenderRelevantChange() noexcept { ++renderRevision_; }
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
    [[nodiscard]] FermentationUiWorkspacePress pressImpl(
        const FermentationUiSnapshot& snapshot,
        const device_platform::DeviceUiTarget& target,
        const ProgramCatalog* catalog);
    [[nodiscard]] bool navigate(FermentationUiWorkspaceSlotAction action);
    [[nodiscard]] bool goBack();
    void setSlot(FermentationUiWorkspaceView& view, std::size_t index,
                 const char* label, FermentationUiWorkspaceSlotAction action,
                 bool enabled = true) const;
    void setCanonicalPageStack(FermentationUiPage page);

    [[nodiscard]] FermentationUiWorkspacePress pressContentCell(
        const FermentationUiSnapshot& snapshot,
        const device_platform::DeviceUiTarget& target,
        const FermentationUiWorkspaceView& current,
        const ProgramCatalog* catalog);
    void cycleStartField(FermentationUiStartField field,
                         const FermentationUiProgramSummaryView& summary);
    void cycleManualField(FermentationUiManualDraftSlot slot,
                          FermentationUiStartField field,
                          const FermentationUiProgramSummaryView& summary);
    [[nodiscard]] static std::optional<FermentationUiManualDraftSlot>
    manualSlotForPage(FermentationUiPage page) noexcept;
    [[nodiscard]] FermentationUiProgramSummaryView makeManualFieldView(
        FermentationUiManualDraftSlot slot) const;
    void applyManualFieldView(FermentationUiWorkspaceView& view,
                              FermentationUiManualDraftSlot slot) const;
    void commitValueEdit();
    [[nodiscard]] FermentationUiSettingsView makeSettingsView(
        const FermentationUiSnapshot& snapshot) const;
    [[nodiscard]] FermentationUiProgramEditView makeProgramEditView(
        const ProgramCatalog* catalog) const;
    [[nodiscard]] const ProgramDocument* storedSelectedProgram(
        const ProgramCatalog* catalog) const;
    // Makes sure the editor works on a candidate copy of the stored program.
    [[nodiscard]] ProgramDocument* ensureProgramCandidate(
        const ProgramCatalog* catalog);
    [[nodiscard]] FermentationUiWorkspacePress pressSettingsRow(
        const FermentationUiSnapshot& snapshot, std::size_t row);
    [[nodiscard]] FermentationUiWorkspacePress pressProgramEditCell(
        const FermentationUiWorkspaceView& current,
        const device_platform::DeviceUiTarget& target,
        const ProgramCatalog* catalog);
    [[nodiscard]] FermentationUiWorkspacePress pressKeyboardCell(
        const device_platform::DeviceUiTarget& target);
    void openTextEdit(FermentationUiTextTarget target, std::string initial);
    [[nodiscard]] FermentationUiWorkspacePress commitTextEdit(
        const FermentationUiSnapshot& snapshot,
        const FermentationUiWorkspaceView& current);

    [[nodiscard]] bool selectedMessageExists(
        const FermentationUiSnapshot& snapshot) const;
    [[nodiscard]] bool selectProgramFor(const std::string& programId,
                                        const ProgramCatalog& catalog,
                                        FermentationUiPage destination);

    FermentationUiPage page_{FermentationUiPage::Home};
    FermentationUiStartField valueEditField_{
        FermentationUiStartField::TargetTemperature};
    NumericEditModel valueEdit_;
    // Where the committed value of the edit page goes: the start candidate or
    // one of the manual drafts.
    FermentationUiManualDraftSlot valueEditSlot_{
        FermentationUiManualDraftSlot::StartCandidate};
    // One draft per FermentationUiManualDraftSlot (indexed by the enumerator;
    // the start candidate slot keeps an unused entry).
    std::array<FermentationUiManualDraft, kFermentationUiManualDraftSlotCount>
        manualDrafts_{};
    // Settings / keyboard / program editor state (S10).
    std::string deviceName_;
    bool deviceNameChangeFailed_{false};
    TextEditModel textEdit_;
    FermentationUiTextTarget textTarget_{FermentationUiTextTarget::DeviceName};
    // Set while the numeric edit page edits a program field (else a start or
    // manual value).
    std::optional<FermentationUiProgramField> valueEditProgramField_;
    // Name entered for a Copy/New request (Edit changes it in the candidate).
    std::optional<std::string> programEditName_;
    bool displayLanguageChangeFailed_{false};
    FermentationUiProgramListIntent programListIntent_{
        FermentationUiProgramListIntent::Start};
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
    std::uint32_t renderRevision_{0U};
};

}  // namespace fermentation
