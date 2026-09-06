#include "fermentation_ui_editing.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <set>
#include <utility>

#include "configuration_limits.hpp"
#include "configuration_text.hpp"

namespace fermentation {
namespace {

bool hasDecimalSeparator(const std::string& value) {
    return value.find('.') != std::string::npos;
}

bool isFactoryProgram(const ProgramDocument& document) {
    return document.program.builtIn && document.program.factoryCatalogEntry;
}

char base36(std::size_t value) {
    return value < 10U ? static_cast<char>('0' + value)
                       : static_cast<char>('a' + value - 10U);
}

}  // namespace

NumericEditModel::NumericEditModel(std::string initial)
    : candidate_(std::move(initial)), committed_(candidate_) {}

bool NumericEditModel::apply(const NumericEditInput& input) noexcept {
    if (state_ != NumericEditState::Editing) return false;
    switch (input.action) {
        case NumericEditAction::Plus:
            if (!candidate_.empty() && candidate_.front() == '-') {
                candidate_.erase(candidate_.begin());
            }
            return true;
        case NumericEditAction::Minus:
            if (candidate_.empty() || candidate_.front() != '-') {
                candidate_.insert(candidate_.begin(), '-');
            } else {
                candidate_.erase(candidate_.begin());
            }
            return true;
        case NumericEditAction::Digit:
            if (input.digit > 9U) return false;
            candidate_.push_back(static_cast<char>('0' + input.digit));
            return true;
        case NumericEditAction::DecimalSeparator:
            if (hasDecimalSeparator(candidate_)) return false;
            if (candidate_.empty() || candidate_ == "-") {
                candidate_ += "0";
            }
            candidate_.push_back('.');
            return true;
        case NumericEditAction::Backspace:
            if (candidate_.empty()) return false;
            candidate_.pop_back();
            return true;
        case NumericEditAction::Clear:
            if (candidate_.empty()) return false;
            candidate_.clear();
            return true;
        case NumericEditAction::Cancel:
            state_ = NumericEditState::Cancelled;
            candidate_.clear();
            return true;
        case NumericEditAction::Commit:
            if (candidate_.empty() || candidate_ == "-") return false;
            committed_ = candidate_;
            state_ = NumericEditState::Committed;
            return true;
    }
    return false;
}

void NumericEditModel::reset(std::string value) {
    candidate_ = std::move(value);
    committed_ = candidate_;
    state_ = NumericEditState::Editing;
}

std::optional<double> NumericEditModel::committedValue() const noexcept {
    if (committed_.empty()) return std::nullopt;
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtod(committed_.c_str(), &end);
    if (errno == ERANGE || end == committed_.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return parsed;
}

TextEditModel::TextEditModel(std::string initial)
    : candidate_(std::move(initial)), committed_(candidate_) {}

bool TextEditModel::apply(const TextEditInput& input) noexcept {
    if (state_ != NumericEditState::Editing) return false;
    switch (input.action) {
        case TextEditAction::Character:
            candidate_.push_back(input.character);
            return true;
        case TextEditAction::Mode:
            mode_ = static_cast<TextEditMode>(
                (static_cast<std::uint8_t>(mode_) + 1U) % 4U);
            return true;
        case TextEditAction::Backspace:
            if (candidate_.empty()) return false;
            candidate_.pop_back();
            return true;
        case TextEditAction::Clear:
            if (candidate_.empty()) return false;
            candidate_.clear();
            return true;
        case TextEditAction::Cancel:
            state_ = NumericEditState::Cancelled;
            candidate_.clear();
            committed_.reset();
            return true;
        case TextEditAction::Commit:
            committed_ = candidate_;
            state_ = NumericEditState::Committed;
            return true;
    }
    return false;
}

void TextEditModel::reset(std::string value) {
    candidate_ = std::move(value);
    committed_ = candidate_;
    state_ = NumericEditState::Editing;
}

UserProgramIdAllocationResult allocateNextUserProgramId(
    const ProgramCatalog& catalog) {
    if (catalog.programs.size() < configuration_limits::kFactoryProgramCount ||
        catalog.programs.size() > configuration_limits::kMaximumProgramCount) {
        return {UserProgramIdAllocationStatus::InvalidCatalog, std::nullopt};
    }
    std::set<std::string> used;
    std::size_t userCount = 0U;
    for (const auto& document : catalog.programs) {
        if (document.program.id.empty() ||
            !used.insert(document.program.id).second) {
            return {UserProgramIdAllocationStatus::InvalidCatalog,
                    std::nullopt};
        }
        if (!isFactoryProgram(document)) ++userCount;
    }
    if (userCount >= configuration_limits::kMaximumUserProgramCount) {
        return {UserProgramIdAllocationStatus::CapacityReached, std::nullopt};
    }
    for (std::size_t ordinal = 0U; ordinal < 36U * 36U; ++ordinal) {
        std::string candidate{"user-"};
        candidate.push_back(base36(ordinal / 36U));
        candidate.push_back(base36(ordinal % 36U));
        if (validateLowercaseIdentifier(
                candidate, configuration_limits::kMinimumProgramIdBytes,
                configuration_limits::kMaximumProgramIdBytes) !=
            ConfigurationTextStatus::Success) {
            return {UserProgramIdAllocationStatus::InvalidCatalog,
                    std::nullopt};
        }
        if (used.find(candidate) == used.end()) {
            return {UserProgramIdAllocationStatus::Allocated, candidate};
        }
    }
    return {UserProgramIdAllocationStatus::CapacityReached, std::nullopt};
}

std::optional<FermentationUiProgramEditSession> openProgramEditSession(
    const RuntimeConfigurationSnapshot& snapshot,
    const std::string& programId) {
    const auto found =
        std::find_if(snapshot.programCatalog().programs.begin(),
                     snapshot.programCatalog().programs.end(),
                     [&programId](const ProgramDocument& document) {
                         return document.program.id == programId;
                     });
    if (found == snapshot.programCatalog().programs.end()) return std::nullopt;
    return FermentationUiProgramEditSession{*found,
                                            snapshot.programCatalogRevision()};
}

ConfigurationPreviewBuildResult beginProgramEditPreview(
    ConfigurationService& service,
    const FermentationUiProgramEditSession& session) {
    return service.beginPreview(session.expectedProgramCatalogRevision);
}

}  // namespace fermentation
