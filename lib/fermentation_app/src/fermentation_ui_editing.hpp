#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "configuration_documents.hpp"
#include "configuration_service.hpp"
#include "device_ui_text.hpp"
#include "run_commands.hpp"
#include "runtime_configuration_snapshot.hpp"

namespace fermentation {

enum class NumericEditAction : std::uint8_t {
    Plus,
    Minus,
    Digit,
    DecimalSeparator,
    Backspace,
    Clear,
    Cancel,
    Commit,
};

enum class NumericEditState : std::uint8_t {
    Editing,
    Committed,
    Cancelled,
};

struct NumericEditInput {
    NumericEditAction action{NumericEditAction::Digit};
    std::uint8_t digit{0U};
};

class NumericEditModel {
   public:
    explicit NumericEditModel(std::string initial = {});

    [[nodiscard]] bool apply(const NumericEditInput& input) noexcept;
    void reset(std::string value = {});
    [[nodiscard]] const std::string& candidate() const noexcept {
        return candidate_;
    }
    [[nodiscard]] NumericEditState state() const noexcept { return state_; }
    [[nodiscard]] std::optional<double> committedValue() const noexcept;

   private:
    std::string candidate_;
    std::string committed_;
    NumericEditState state_{NumericEditState::Editing};
};

enum class TextEditAction : std::uint8_t {
    Character,
    Mode,
    Backspace,
    Clear,
    Cancel,
    Commit,
};

enum class TextEditMode : std::uint8_t {
    Lowercase,
    Uppercase,
    Digits,
    Symbols,
};

struct TextEditInput {
    TextEditAction action{TextEditAction::Character};
    char character{' '};
};

class TextEditModel {
   public:
    explicit TextEditModel(std::string initial = {});

    [[nodiscard]] bool apply(const TextEditInput& input) noexcept;
    void reset(std::string value = {});
    [[nodiscard]] const std::string& candidate() const noexcept {
        return candidate_;
    }
    [[nodiscard]] TextEditMode mode() const noexcept { return mode_; }
    [[nodiscard]] NumericEditState state() const noexcept { return state_; }
    [[nodiscard]] const std::optional<std::string>& committedValue()
        const noexcept {
        return committed_;
    }

   private:
    std::string candidate_;
    std::optional<std::string> committed_;
    TextEditMode mode_{TextEditMode::Lowercase};
    NumericEditState state_{NumericEditState::Editing};
};

enum class UserProgramIdAllocationStatus : std::uint8_t {
    Allocated,
    InvalidCatalog,
    CapacityReached,
};

struct UserProgramIdAllocationResult {
    UserProgramIdAllocationStatus status{
        UserProgramIdAllocationStatus::InvalidCatalog};
    std::optional<std::string> id;
};

[[nodiscard]] UserProgramIdAllocationResult allocateNextUserProgramId(
    const ProgramCatalog& catalog);

struct FermentationUiProgramListEntry {
    ProgramDocument program;
    bool active{false};
    bool startable{false};
    std::optional<device_platform::TextKey> blockedReason;
};

// The list is a read-only projection of the active canonical catalog. It
// preserves factory-before-user ordering, excludes uninstalled entries, and
// exposes invalid installed entries with a typed owning lock reason.
[[nodiscard]] std::vector<FermentationUiProgramListEntry>
makeFermentationUiProgramList(const ProgramCatalog& catalog);

// The current run owner derives this evidence from the canonical
// RunCommandState. It is deliberately not part of the UI edit request and
// cannot be authored by the workspace.
class FermentationUiProgramUsageEvidence {
   public:
    [[nodiscard]] bool isInUse(const std::string& programId) const noexcept;

   private:
    explicit FermentationUiProgramUsageEvidence(
        std::optional<std::string> activeProgramId)
        : activeProgramId_(std::move(activeProgramId)) {}
    friend FermentationUiProgramUsageEvidence
    makeFermentationUiProgramUsageEvidence(const RunCommandState& runState);

    std::optional<std::string> activeProgramId_;
};

[[nodiscard]] FermentationUiProgramUsageEvidence
makeFermentationUiProgramUsageEvidence(const RunCommandState& runState);

enum class FermentationUiProgramEditOperation : std::uint8_t {
    Edit,
    Copy,
    New,
    Reset,
    Uninstall,
    Delete,
};

enum class FermentationUiProgramEditStatus : std::uint8_t {
    Applied,
    ConfirmationRequired,
    NotFound,
    NotAllowed,
    InvalidCandidate,
    CapacityReached,
};

struct FermentationUiProgramEditRequest {
    FermentationUiProgramEditOperation operation{
        FermentationUiProgramEditOperation::Edit};
    std::string programId;
    std::optional<ProgramDocument> candidate;
    std::optional<std::string> name;
    bool confirmed{false};
};

struct FermentationUiProgramEditResult {
    FermentationUiProgramEditStatus status{
        FermentationUiProgramEditStatus::InvalidCandidate};
    std::optional<std::string> affectedProgramId;
};

// Applies one operation to a preview-bound catalog only. Persistence remains
// ConfigurationService::installPreview/confirmPreview ownership.
[[nodiscard]] FermentationUiProgramEditResult applyProgramEdit(
    ProgramCatalog& catalog, const FermentationUiProgramEditRequest& request,
    const FermentationUiProgramUsageEvidence& usage);

[[nodiscard]] ConfigurationPreviewInstallResult applyProgramEditPreview(
    ConfigurationService& service, ProgramCatalogRevision expectedRevision,
    const FermentationUiProgramEditRequest& request,
    const FermentationUiProgramUsageEvidence& usage);

struct FermentationUiProgramEditSession {
    ProgramDocument candidate;
    ProgramCatalogRevision expectedProgramCatalogRevision;
};

[[nodiscard]] std::optional<FermentationUiProgramEditSession>
openProgramEditSession(const RuntimeConfigurationSnapshot& snapshot,
                       const std::string& programId);

[[nodiscard]] ConfigurationPreviewBuildResult beginProgramEditPreview(
    ConfigurationService& service,
    const FermentationUiProgramEditSession& session);

}  // namespace fermentation
