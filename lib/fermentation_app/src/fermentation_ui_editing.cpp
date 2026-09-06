#include "fermentation_ui_editing.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <set>
#include <utility>

#include "configuration_limits.hpp"
#include "configuration_text.hpp"
#include "fermentation_ui_text.hpp"
#include "standard_program_catalog.hpp"

namespace fermentation {
namespace {

bool hasDecimalSeparator(const std::string& value) {
    return value.find('.') != std::string::npos;
}

bool isFactoryProgram(const ProgramDocument& document) {
    return document.program.builtIn && document.program.factoryCatalogEntry;
}

std::optional<std::size_t> findProgramIndex(const ProgramCatalog& catalog,
                                            const std::string& id) {
    for (std::size_t i = 0U; i < catalog.programs.size(); ++i) {
        if (catalog.programs[i].program.id == id) return i;
    }
    return std::nullopt;
}

bool isDeletionOperation(FermentationUiProgramEditOperation operation) {
    return operation == FermentationUiProgramEditOperation::Delete ||
           operation == FermentationUiProgramEditOperation::Uninstall;
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

std::vector<FermentationUiProgramListEntry> makeFermentationUiProgramList(
    const ProgramCatalog& catalog) {
    std::vector<ProgramDocument> ordered;
    ordered.reserve(catalog.programs.size());
    for (const auto& program : catalog.programs) ordered.push_back(program);
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto& left, const auto& right) {
                         return isFactoryProgram(left) &&
                                !isFactoryProgram(right);
                     });

    std::vector<FermentationUiProgramListEntry> result;
    result.reserve(ordered.size());
    for (auto& document : ordered) {
        const bool active = document.program.installed;
        if (!active) continue;
        const bool startable =
            active && document.program.enabled &&
            validateProgram(document, ValidationPurpose::Runnable).valid();
        std::optional<device_platform::TextKey> blockedReason;
        if (!document.program.enabled) {
            blockedReason = fermentationTextKey("program-disabled");
        } else if (!startable) {
            blockedReason = fermentationTextKey("program-invalid");
        }
        result.push_back(FermentationUiProgramListEntry{
            std::move(document), active, startable, std::move(blockedReason)});
    }
    return result;
}

bool FermentationUiProgramUsageEvidence::isInUse(
    const std::string& programId) const noexcept {
    return activeProgramId_.has_value() && *activeProgramId_ == programId;
}

FermentationUiProgramUsageEvidence makeFermentationUiProgramUsageEvidence(
    const RunCommandState& runState) {
    std::optional<std::string> activeProgramId;
    if (runState.activeProgramRun.has_value()) {
        if (const auto* program =
                storedProgram(runState.activeProgramRun->snapshot().source)) {
            activeProgramId = program->program.id;
        }
    }
    return FermentationUiProgramUsageEvidence{std::move(activeProgramId)};
}

FermentationUiProgramEditResult applyProgramEdit(
    ProgramCatalog& catalog, const FermentationUiProgramEditRequest& request,
    const FermentationUiProgramUsageEvidence& usage) {
    const auto found = findProgramIndex(catalog, request.programId);
    switch (request.operation) {
        case FermentationUiProgramEditOperation::New: {
            const auto allocation = allocateNextUserProgramId(catalog);
            if (allocation.status ==
                UserProgramIdAllocationStatus::CapacityReached)
                return {FermentationUiProgramEditStatus::CapacityReached,
                        std::nullopt};
            if (!allocation.id.has_value())
                return {FermentationUiProgramEditStatus::InvalidCandidate,
                        std::nullopt};
            auto templateProgram = FactoryProgramCatalog::find("water-kefir");
            if (!templateProgram.has_value())
                return {FermentationUiProgramEditStatus::InvalidCandidate,
                        std::nullopt};
            auto created = request.candidate.has_value()
                               ? *request.candidate
                               : std::move(*templateProgram);
            created.program.id = *allocation.id;
            if (request.name.has_value()) created.program.name = *request.name;
            if (created.program.name.empty())
                created.program.name = "New program";
            created.program.builtIn = false;
            created.program.factoryCatalogEntry = false;
            created.program.resettable = false;
            created.program.userDeletable = true;
            created.program.installed = true;
            if (!request.candidate.has_value()) {
                created.program.notes.clear();
                created.program.fermentationStages.front()
                    .targetTemperatureCelsius.reset();
                created.program.fermentationStages.front()
                    .durationMinutes.reset();
                created.program.targetQualification.bandCelsius.reset();
                created.program.targetQualification.durationMinutes.reset();
                created.program.maximumTargetReachMinutes.reset();
                created.program.maximumProductWaitMinutes.reset();
                created.program.completion.mode =
                    CompletionMode::FinishWithoutCooling;
                created.program.completion.coolingTargetCelsius.reset();
                created.program.completion.holdDurationMinutes.reset();
            }
            catalog.programs.push_back(std::move(created));
            return {FermentationUiProgramEditStatus::Applied, *allocation.id};
        }
        case FermentationUiProgramEditOperation::Copy: {
            if (!found.has_value())
                return {FermentationUiProgramEditStatus::NotFound,
                        std::nullopt};
            const auto allocation = allocateNextUserProgramId(catalog);
            if (allocation.status ==
                UserProgramIdAllocationStatus::CapacityReached)
                return {FermentationUiProgramEditStatus::CapacityReached,
                        std::nullopt};
            if (!allocation.id.has_value())
                return {FermentationUiProgramEditStatus::InvalidCandidate,
                        std::nullopt};
            auto copy = catalog.programs[*found];
            copy.program.id = *allocation.id;
            copy.program.name =
                request.name.value_or(copy.program.name + " copy");
            copy.program.builtIn = false;
            copy.program.factoryCatalogEntry = false;
            copy.program.resettable = false;
            copy.program.userDeletable = true;
            copy.program.installed = true;
            catalog.programs.push_back(std::move(copy));
            return {FermentationUiProgramEditStatus::Applied, *allocation.id};
        }
        case FermentationUiProgramEditOperation::Reset: {
            if (!found.has_value())
                return {FermentationUiProgramEditStatus::NotFound,
                        std::nullopt};
            if (!isFactoryProgram(catalog.programs[*found]) ||
                !catalog.programs[*found].program.resettable) {
                return {FermentationUiProgramEditStatus::NotAllowed,
                        std::nullopt};
            }
            const auto factory = FactoryProgramCatalog::find(request.programId);
            if (!factory.has_value())
                return {FermentationUiProgramEditStatus::InvalidCandidate,
                        std::nullopt};
            catalog.programs[*found] = *factory;
            return {FermentationUiProgramEditStatus::Applied,
                    request.programId};
        }
        case FermentationUiProgramEditOperation::Uninstall: {
            if (!found.has_value())
                return {FermentationUiProgramEditStatus::NotFound,
                        std::nullopt};
            auto& program = catalog.programs[*found].program;
            if (!request.confirmed)
                return {FermentationUiProgramEditStatus::ConfirmationRequired,
                        request.programId};
            if (!isFactoryProgram(catalog.programs[*found]) ||
                !program.userDeletable || usage.isInUse(request.programId)) {
                return {FermentationUiProgramEditStatus::NotAllowed,
                        std::nullopt};
            }
            program.installed = false;
            return {FermentationUiProgramEditStatus::Applied,
                    request.programId};
        }
        case FermentationUiProgramEditOperation::Delete: {
            if (!found.has_value())
                return {FermentationUiProgramEditStatus::NotFound,
                        std::nullopt};
            if (!request.confirmed)
                return {FermentationUiProgramEditStatus::ConfirmationRequired,
                        request.programId};
            const auto& program = catalog.programs[*found].program;
            if (*found < configuration_limits::kFactoryProgramCount ||
                !program.userDeletable || usage.isInUse(request.programId)) {
                return {FermentationUiProgramEditStatus::NotAllowed,
                        std::nullopt};
            }
            catalog.programs.erase(catalog.programs.begin() +
                                   static_cast<std::ptrdiff_t>(*found));
            return {FermentationUiProgramEditStatus::Applied,
                    request.programId};
        }
        case FermentationUiProgramEditOperation::Edit: {
            if (!found.has_value())
                return {FermentationUiProgramEditStatus::NotFound,
                        std::nullopt};
            if (!request.candidate.has_value() ||
                request.candidate->program.id != request.programId) {
                return {FermentationUiProgramEditStatus::InvalidCandidate,
                        std::nullopt};
            }
            const auto& current = catalog.programs[*found].program;
            const auto& candidate = request.candidate->program;
            if (current.builtIn != candidate.builtIn ||
                current.factoryCatalogEntry != candidate.factoryCatalogEntry ||
                current.resettable != candidate.resettable ||
                current.userDeletable != candidate.userDeletable) {
                return {FermentationUiProgramEditStatus::NotAllowed,
                        std::nullopt};
            }
            catalog.programs[*found] = *request.candidate;
            return {FermentationUiProgramEditStatus::Applied,
                    request.programId};
        }
    }
    return {FermentationUiProgramEditStatus::InvalidCandidate, std::nullopt};
}

ConfigurationPreviewInstallResult applyProgramEditPreview(
    ConfigurationService& service, ProgramCatalogRevision expectedRevision,
    const FermentationUiProgramEditRequest& request,
    const FermentationUiProgramUsageEvidence& usage) {
    if (isDeletionOperation(request.operation) &&
        usage.isInUse(request.programId)) {
        return {ConfigurationPreviewStatus::NotAllowed, std::nullopt};
    }
    auto build = service.beginPreview(expectedRevision);
    if (build.status != ConfigurationPreviewStatus::Success)
        return {build.status, std::nullopt};
    const auto mutation =
        applyProgramEdit(build.lease.programCatalog(), request, usage);
    if (mutation.status != FermentationUiProgramEditStatus::Applied)
        return {ConfigurationPreviewStatus::InvalidCandidate, std::nullopt};
    const ChangeOperation operation{
        request.operation == FermentationUiProgramEditOperation::Reset
            ? ChangeOperationKind::StandardProgramReset
            : ChangeOperationKind::NormalEdit,
        0U};
    return service.installPreview(std::move(build.lease),
                                  {ChangeOriginKind::LocalDisplay, 0U},
                                  operation);
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
