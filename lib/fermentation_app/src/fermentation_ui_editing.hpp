#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "configuration_documents.hpp"
#include "configuration_service.hpp"
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
