#include "fermentation_touch_workspace.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "fermentation_ui_editing.hpp"
#include "fermentation_ui_text.hpp"

namespace fermentation {

namespace {

// The canonical decision-required message, exactly as the Application-owned
// projection defines it for the Waiting home mode (fermentation_ui_projector):
// an earlier active, unresolved message of another kind must not be taken for
// it.
bool isCanonicalDecisionRequiredMessage(const RuntimeMessage& message) {
    return message.active && !message.resolved && message.decisionRequired &&
           message.messageClass == MessageClass::DecisionRequired &&
           (message.code == MessageCode::UserDecisionRequired ||
            message.code == MessageCode::ProductInsertionRequested);
}

std::vector<FermentationUiSafeBootCapability>
safeBootUnavailableCapabilities() {
    return {FermentationUiSafeBootCapability::PersistentFactoryReset,
            FermentationUiSafeBootCapability::RawTouchRecovery,
            FermentationUiSafeBootCapability::NetworkProvisioningRecovery,
            FermentationUiSafeBootCapability::DiagnosticsExport};
}

// Validation field of each numeric start value (the program validator names
// the field of every error, so the commit check reuses it instead of keeping
// parallel limits).
const char* numericStartFieldName(FermentationUiStartField field) noexcept {
    switch (field) {
        case FermentationUiStartField::TargetTemperature:
            return "defaults.fermentation_temperature_c";
        case FermentationUiStartField::Duration:
            return "defaults.fermentation_duration_min";
        case FermentationUiStartField::CoolingTarget:
            return "defaults.completion.cooling_target_c";
        case FermentationUiStartField::HoldDuration:
            return "defaults.completion.hold_duration_min";
        case FermentationUiStartField::Preheat:
        case FermentationUiStartField::SensorMode:
        case FermentationUiStartField::CompletionMode:
            break;
    }
    return "";
}

// Sets one numeric next-run override on the candidate.
void setNumericStartOverride(FermentationUiStartCandidate& candidate,
                             FermentationUiStartField field,
                             double value) noexcept {
    switch (field) {
        case FermentationUiStartField::TargetTemperature:
            candidate.targetTemperatureCelsius = value;
            break;
        case FermentationUiStartField::Duration:
            candidate.fermentationDurationMinutes =
                static_cast<std::uint32_t>(value);
            break;
        case FermentationUiStartField::CoolingTarget:
            candidate.coolingTargetCelsius = value;
            break;
        case FermentationUiStartField::HoldDuration:
            candidate.holdDurationMinutes = static_cast<std::uint32_t>(value);
            break;
        case FermentationUiStartField::Preheat:
        case FermentationUiStartField::SensorMode:
        case FermentationUiStartField::CompletionMode:
            break;
    }
}

// A value is accepted for the field when the existing program validator
// reports no error for exactly that field on the program with the candidate
// applied. Errors of other fields (for example a completion mode that still
// lacks its cooling target) do not block this commit; they keep `confirm`
// disabled through `valuesValid`.
bool numericStartValueAccepted(const ProgramDocument& stored,
                               FermentationUiStartCandidate candidate,
                               FermentationUiStartField field, double value) {
    constexpr double kWholeNumberLimit = 4'000'000'000.0;
    if (!std::isfinite(value)) return false;
    if (isWholeNumberStartField(field) &&
        (value < 0.0 || value > kWholeNumberLimit ||
         value != std::floor(value))) {
        return false;
    }
    setNumericStartOverride(candidate, field, value);
    auto effective = stored;
    applyStartCandidateOverrides(effective, candidate);
    const std::string_view name = numericStartFieldName(field);
    const auto result = validateProgram(effective, ValidationPurpose::Runnable);
    return std::none_of(
        result.errors.begin(), result.errors.end(),
        [name](const ValidationError& error) { return name == error.field; });
}

const char* valueEditTitleKey(FermentationUiStartField field) noexcept {
    switch (field) {
        case FermentationUiStartField::TargetTemperature:
            return "field-target";
        case FermentationUiStartField::Duration:
            return "field-duration";
        case FermentationUiStartField::CoolingTarget:
            return "field-cooling";
        case FermentationUiStartField::HoldDuration:
            return "field-hold";
        case FermentationUiStartField::Preheat:
        case FermentationUiStartField::SensorMode:
        case FermentationUiStartField::CompletionMode:
            break;
    }
    return "start";
}

constexpr std::size_t kMaxValueEditCharacters = 8U;

// Keypad (D8): rows 0..2 are digits 1..9, row 3 is `.`, `0`, `+/-`. Whole-
// number fields have no decimal separator or sign; input length is bounded.
bool keypadKeyEnabled(std::uint8_t row, std::uint8_t column,
                      FermentationUiStartField field,
                      const std::string& candidate) noexcept {
    if (row >= kFermentationUiKeypadRows ||
        column >= kFermentationUiKeypadColumns) {
        return false;
    }
    if (row == 3U && column != 1U) {
        if (isWholeNumberStartField(field)) return false;
        if (column == 0U) {
            return candidate.size() < kMaxValueEditCharacters &&
                   candidate.find('.') == std::string::npos;
        }
        return true;
    }
    return candidate.size() < kMaxValueEditCharacters;
}

std::string startValueText(FermentationUiStartField field,
                           const FermentationUiProgramSummaryView& summary) {
    char buffer[24];
    switch (field) {
        case FermentationUiStartField::TargetTemperature:
            if (!summary.targetTemperatureCelsius.has_value()) return {};
            std::snprintf(buffer, sizeof(buffer), "%.1f",
                          *summary.targetTemperatureCelsius);
            return buffer;
        case FermentationUiStartField::CoolingTarget:
            if (!summary.coolingTargetCelsius.has_value()) return {};
            std::snprintf(buffer, sizeof(buffer), "%.1f",
                          *summary.coolingTargetCelsius);
            return buffer;
        case FermentationUiStartField::Duration:
            if (!summary.durationMinutes.has_value()) return {};
            return std::to_string(*summary.durationMinutes);
        case FermentationUiStartField::HoldDuration:
            if (!summary.holdDurationMinutes.has_value()) return {};
            return std::to_string(*summary.holdDurationMinutes);
        case FermentationUiStartField::Preheat:
        case FermentationUiStartField::SensorMode:
        case FermentationUiStartField::CompletionMode:
            break;
    }
    return {};
}

// The next requested mode of the sensor cycle: none (the stored preference's
// canonical default) -> each other structurally allowed mode -> none. The
// rule comes from the #21 start matrix; nothing is derived from sensor
// evidence here. Returns nullopt when the preference has no alternative.
std::optional<std::optional<RunSensorMode>> nextSensorOverride(
    SensorPreference preference,
    const std::optional<RunSensorMode>& current) noexcept {
    const auto fallback = defaultProgramStartSensorMode(preference);
    if (!fallback.has_value()) return std::nullopt;
    std::array<RunSensorMode, 2U> options{};
    std::size_t count = 0U;
    for (const auto mode : {RunSensorMode::Air, RunSensorMode::Product}) {
        if (mode != *fallback &&
            programStartSensorModeAllowed(preference, mode))
            options[count++] = mode;
    }
    if (count == 0U) return std::nullopt;
    if (!current.has_value()) return std::optional<RunSensorMode>{options[0]};
    for (std::size_t index = 0U; index + 1U < count; ++index) {
        if (options[index] == *current)
            return std::optional<RunSensorMode>{options[index + 1U]};
    }
    return std::optional<RunSensorMode>{std::nullopt};
}

// Display projection for ProgramSummary: the stored program with the
// next-run candidate overrides applied by the single shared definition
// (applyStartCandidateOverrides). The stage values follow the first stage; a
// missing value stays absent (rendered as "--"), never 0. The display never
// mutates the stored program.
FermentationUiProgramSummaryView makeProgramSummary(
    const ProgramDocument& document,
    const FermentationUiStartCandidate* candidate, bool startable) {
    auto effective = document;
    if (candidate != nullptr)
        applyStartCandidateOverrides(effective, *candidate);
    const auto& stored = document.program;
    const auto& program = effective.program;
    FermentationUiProgramSummaryView summary;
    summary.name = program.name;
    if (!program.fermentationStages.empty()) {
        summary.targetTemperatureCelsius =
            program.fermentationStages.front().targetTemperatureCelsius;
        summary.durationMinutes =
            program.fermentationStages.front().durationMinutes;
    }
    summary.preheat = program.preheat;
    summary.sensorPreference = program.sensorPreference;
    summary.completionMode = program.completion.mode;
    summary.coolingTargetCelsius = program.completion.coolingTargetCelsius;
    summary.holdDurationMinutes = program.completion.holdDurationMinutes;
    // An override equal to the preference's canonical default is no override.
    const auto sensorDefault =
        defaultProgramStartSensorMode(program.sensorPreference);
    if (candidate != nullptr && candidate->sensorMode.has_value() &&
        candidate->sensorMode != sensorDefault) {
        summary.sensorModeOverride = candidate->sensorMode;
    }

    const auto storedStage = stored.fermentationStages.empty()
                                 ? FermentationStage{}
                                 : stored.fermentationStages.front();
    const auto index = [](FermentationUiStartField field) {
        return static_cast<std::size_t>(field);
    };
    summary.changed[index(FermentationUiStartField::TargetTemperature)] =
        summary.targetTemperatureCelsius !=
        storedStage.targetTemperatureCelsius;
    summary.changed[index(FermentationUiStartField::Duration)] =
        summary.durationMinutes != storedStage.durationMinutes;
    summary.changed[index(FermentationUiStartField::Preheat)] =
        summary.preheat != stored.preheat;
    summary.changed[index(FermentationUiStartField::SensorMode)] =
        summary.sensorModeOverride.has_value();
    summary.changed[index(FermentationUiStartField::CompletionMode)] =
        summary.completionMode != stored.completion.mode;
    summary.changed[index(FermentationUiStartField::CoolingTarget)] =
        summary.coolingTargetCelsius != stored.completion.coolingTargetCelsius;
    summary.changed[index(FermentationUiStartField::HoldDuration)] =
        summary.holdDurationMinutes != stored.completion.holdDurationMinutes;

    summary.fields[summary.fieldCount++] =
        FermentationUiStartField::TargetTemperature;
    summary.fields[summary.fieldCount++] = FermentationUiStartField::Duration;
    summary.fields[summary.fieldCount++] = FermentationUiStartField::Preheat;
    summary.fields[summary.fieldCount++] = FermentationUiStartField::SensorMode;
    summary.fields[summary.fieldCount++] =
        FermentationUiStartField::CompletionMode;
    if (summary.completionMode != CompletionMode::FinishWithoutCooling) {
        summary.fields[summary.fieldCount++] =
            FermentationUiStartField::CoolingTarget;
    }
    if (summary.completionMode == CompletionMode::CoolAndHoldForDuration) {
        summary.fields[summary.fieldCount++] =
            FermentationUiStartField::HoldDuration;
    }
    summary.editable = startable && candidate != nullptr;
    // The structural #21 rule (preference + requested mode) is part of the
    // start values; evidence-dependent decisions stay with the start owner.
    const bool sensorAllowed =
        candidate == nullptr || !candidate->sensorMode.has_value() ||
        programStartSensorModeAllowed(program.sensorPreference,
                                      *candidate->sensorMode);
    summary.valuesValid =
        sensorAllowed &&
        validateProgram(effective, ValidationPurpose::Runnable).valid();
    return summary;
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
        case FermentationUiWorkspaceSlotAction::ValueEditCancel:
        case FermentationUiWorkspaceSlotAction::ValueEditBackspace:
        case FermentationUiWorkspaceSlotAction::ValueEditClear:
        case FermentationUiWorkspaceSlotAction::ValueEditCommit:
        case FermentationUiWorkspaceSlotAction::ResetStartValues:
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
        case FermentationUiPage::ValueEdit:
            route.push_back(key("programs"));
            route.push_back(key("edit"));
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
        case FermentationUiPage::ValueEdit:
            pageStack_.insert(pageStack_.end(),
                              {FermentationUiPage::ProgramList,
                               FermentationUiPage::ProgramSummary,
                               FermentationUiPage::ValueEdit});
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
    displayLanguageChangeFailed_ = false;
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
            } else if (catalog != nullptr && view.programList.empty()) {
                // An empty active list is a valid catalog state; without a
                // row to pick, `new` must stay reachable for administration.
                setSlot(view, 3U, "new",
                        FermentationUiWorkspaceSlotAction::NewProgram);
            }
            break;
        case FermentationUiPage::ProgramSummary: {
            view.title = key("start");
            // A listed program that cannot be started stays selectable for
            // administration; the owning list projection names the reason.
            bool resetOffered = false;
            if (catalog != nullptr && selectedProgramId_.has_value()) {
                const auto entries = makeFermentationUiProgramList(*catalog);
                const auto selected = std::find_if(
                    entries.begin(), entries.end(),
                    [this](const FermentationUiProgramListEntry& entry) {
                        return entry.program.program.id == *selectedProgramId_;
                    });
                if (selected != entries.end() && !selected->startable)
                    view.blockedReason = selected->blockedReason;
                if (selected != entries.end()) {
                    view.programSummary = makeProgramSummary(
                        selected->program,
                        selectedCandidate_.programId == *selectedProgramId_
                            ? &selectedCandidate_
                            : nullptr,
                        selected->startable);
                    const auto& summary = *view.programSummary;
                    view.pager.itemCount = summary.fieldCount;
                    view.pager.currentIndex =
                        summary.fieldCount == 0U
                            ? 0U
                            : std::min(pager_.currentIndex,
                                       summary.fieldCount - 1U);
                    if (selected->startable && !summary.valuesValid)
                        view.blockedReason = key("start-values-invalid");
                    // O8 (a): reset only when the candidate differs from the
                    // stored program values.
                    resetOffered =
                        summary.editable &&
                        std::any_of(summary.changed.begin(),
                                    summary.changed.end(),
                                    [](bool changed) { return changed; });
                }
            }
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "edit",
                    FermentationUiWorkspaceSlotAction::NavigateProgramActions);
            setSlot(view, 2U, "confirm",
                    FermentationUiWorkspaceSlotAction::StartProgram,
                    selectedProgramId_.has_value() &&
                        selectedCandidate_.programId == *selectedProgramId_ &&
                        view.programSummary.has_value() &&
                        view.programSummary->valuesValid);
            if (resetOffered) {
                setSlot(view, 3U, "reset",
                        FermentationUiWorkspaceSlotAction::ResetStartValues);
            } else {
                setSlot(view, 3U, "status",
                        FermentationUiWorkspaceSlotAction::NavigateStatus);
            }
            break;
        }
        case FermentationUiPage::ValueEdit: {
            view.title = key(valueEditTitleKey(valueEditField_));
            view.pager.currentIndex = pager_.currentIndex;
            FermentationUiValueEditView edit;
            edit.field = valueEditField_;
            edit.candidate = valueEdit_.candidate();
            if (catalog != nullptr && selectedProgramId_.has_value()) {
                const auto found = std::find_if(
                    catalog->programs.begin(), catalog->programs.end(),
                    [this](const ProgramDocument& document) {
                        return document.program.id == *selectedProgramId_;
                    });
                auto probe = valueEdit_;
                if (found != catalog->programs.end() &&
                    probe.apply({NumericEditAction::Commit, 0U}) &&
                    probe.committedValue().has_value()) {
                    edit.commitValid = numericStartValueAccepted(
                        *found, selectedCandidate_, valueEditField_,
                        *probe.committedValue());
                }
            }
            view.valueEdit = std::move(edit);
            setSlot(view, 0U, "cancel",
                    FermentationUiWorkspaceSlotAction::ValueEditCancel);
            setSlot(view, 1U, "backspace",
                    FermentationUiWorkspaceSlotAction::ValueEditBackspace,
                    !valueEdit_.candidate().empty());
            setSlot(view, 2U, "clear",
                    FermentationUiWorkspaceSlotAction::ValueEditClear,
                    !valueEdit_.candidate().empty());
            setSlot(view, 3U, "ok",
                    FermentationUiWorkspaceSlotAction::ValueEditCommit,
                    view.valueEdit->commitValid);
            break;
        }
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
            view.pager.itemCount = snapshot.temperatures.size();
            view.pager.currentIndex = pager_.currentIndex;
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
            // The slot opens the explicitly selected message (a row tap does
            // so directly); it never substitutes another message.
            setSlot(view, 3U, "details",
                    FermentationUiWorkspaceSlotAction::NavigateMessageDetail,
                    selectedMessageExists(snapshot));
            break;
        case FermentationUiPage::MessageDetail:
            view.title = key("message-detail");
            // A selection that is no longer in the snapshot shows no detail
            // and offers no message action.
            if (selectedMessageExists(snapshot))
                view.selectedMessageId = selectedMessageId_;
            setSlot(view, 1U, "acknowledge",
                    FermentationUiWorkspaceSlotAction::AcknowledgeMessage,
                    selectedMessageExists(snapshot));
            setSlot(view, 2U, "mute",
                    FermentationUiWorkspaceSlotAction::MuteMessage,
                    selectedMessageExists(snapshot));
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
            // The time correction has no R1 user path (plan 4.1): the slot is
            // never offered, even if the test helper staged a value.
            setSlot(view, 1U, "resume-fallback",
                    FermentationUiWorkspaceSlotAction::ResumeFallback,
                    fallbackAllowed);
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
            // One selectable row per language included in this build.
            view.pager.itemCount =
                makeFermentationR1DeviceUiBuildCatalog().includedLocales.size();
            // A refused change stays visible on this page until the next
            // outcome or until the page is left.
            if (displayLanguageChangeFailed_)
                view.blockedReason = key("language-change-failed");
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
    pager_.currentIndex = 0U;
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
    displayLanguageChangeFailed_ = false;
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
    displayLanguageChangeFailed_ = false;
    programEditDirty_ = false;
    return true;
}

bool FermentationTouchWorkspace::selectedMessageExists(
    const FermentationUiSnapshot& snapshot) const {
    return selectedMessageId_.has_value() &&
           std::any_of(snapshot.messages.begin(), snapshot.messages.end(),
                       [this](const MessageView& message) {
                           return message.message.id == *selectedMessageId_;
                       });
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
            // From the message list the explicit selection stands; the first
            // decision-required message is only the default entry from the
            // waiting home view.
            if (page_ != FermentationUiPage::Messages &&
                !snapshot.messages.empty()) {
                const auto decision = std::find_if(
                    snapshot.messages.begin(), snapshot.messages.end(),
                    [](const MessageView& message) {
                        return isCanonicalDecisionRequiredMessage(
                            message.message);
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
            if (selectedMessageExists(snapshot))
                result.action = FermentationUiEnvelopePayload{
                    FermentationUiAcknowledgeMessageIntent{
                        *selectedMessageId_}};
            break;
        case FermentationUiWorkspaceSlotAction::MuteMessage:
            if (selectedMessageExists(snapshot))
                result.action = FermentationUiEnvelopePayload{
                    FermentationUiMuteMessageIntent{*selectedMessageId_}};
            break;
        case FermentationUiWorkspaceSlotAction::ResetFault:
            result.action =
                FermentationUiEnvelopePayload{FermentationUiResetFaultIntent{}};
            break;
        case FermentationUiWorkspaceSlotAction::NavigateBack:
        case FermentationUiWorkspaceSlotAction::ValueEditCancel:
            result.navigated = goBack();
            break;
        case FermentationUiWorkspaceSlotAction::ValueEditBackspace:
            result.navigated =
                valueEdit_.apply({NumericEditAction::Backspace, 0U});
            break;
        case FermentationUiWorkspaceSlotAction::ValueEditClear:
            result.navigated = valueEdit_.apply({NumericEditAction::Clear, 0U});
            break;
        case FermentationUiWorkspaceSlotAction::ValueEditCommit:
            commitValueEdit();
            result.navigated = true;
            break;
        case FermentationUiWorkspaceSlotAction::ResetStartValues:
            // Back to the stored program values: only the identity stays.
            if (selectedProgramId_.has_value()) {
                selectedCandidate_ = {};
                selectedCandidate_.programId = *selectedProgramId_;
                result.navigated = true;
            }
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
        if (page_ == FermentationUiPage::ProgramSummary) {
            // Rows are the visible window of the start fields (column 0, only
            // while editable); column 1 holds the two pager buttons.
            const auto& summary = current.programSummary;
            const auto first = current.pager.currentIndex;
            enabled =
                summary.has_value() &&
                ((target.column == 0U && summary->editable &&
                  target.row < kFermentationUiListVisibleRows &&
                  first + target.row < summary->fieldCount &&
                  (summary->fields[first + target.row] !=
                       FermentationUiStartField::SensorMode ||
                   nextSensorOverride(summary->sensorPreference, std::nullopt)
                       .has_value())) ||
                 (target.column == 1U && target.row == 0U &&
                  current.pager.canMoveUp()) ||
                 (target.column == 1U && target.row == 1U &&
                  current.pager.canMoveDown()));
        } else if (page_ == FermentationUiPage::ValueEdit) {
            enabled = current.valueEdit.has_value() &&
                      keypadKeyEnabled(target.row, target.column,
                                       current.valueEdit->field,
                                       current.valueEdit->candidate);
        } else
            enabled = target.column == 0U &&
                      target.row < kFermentationUiListVisibleRows &&
                      current.pager.currentIndex + target.row <
                          (page_ == FermentationUiPage::ProgramList
                               ? current.programList.size()
                           : page_ == FermentationUiPage::Messages
                               ? snapshot.messages.size()
                           : page_ == FermentationUiPage::HeaderLanguage
                               ? current.pager.itemCount
                               : std::size_t{0U});
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
            if (page_ == FermentationUiPage::ProgramSummary ||
                page_ == FermentationUiPage::ValueEdit) {
                auto pressed =
                    pressContentCell(snapshot, target, current, catalog);
                pressed.interaction = result.interaction;
                return pressed;
            }
            if (page_ == FermentationUiPage::HeaderLanguage) {
                // `enabled` guarantees an in-range language row. The press
                // only carries intent; the Application owns the catalog
                // check and the persistent change.
                result.setDisplayLanguage =
                    FermentationUiSetDisplayLanguageCommand{
                        makeFermentationR1DeviceUiBuildCatalog()
                            .includedLocales[target.row]
                            .value(),
                        snapshot.revisions.expectedUserConfigurationRevision};
                return result;
            }
            if (page_ == FermentationUiPage::Messages) {
                // `enabled` guarantees an in-range message; its canonical id
                // becomes the selection.
                selectedMessageId_ =
                    snapshot.messages[current.pager.currentIndex + target.row]
                        .message.id;
                result.navigated = navigate(
                    FermentationUiWorkspaceSlotAction::NavigateMessageDetail);
                return result;
            }
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
                case FermentationUiPage::ValueEdit:
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

FermentationUiWorkspacePress FermentationTouchWorkspace::pressContentCell(
    const FermentationUiSnapshot& /*snapshot*/,
    const device_platform::DeviceUiTarget& target,
    const FermentationUiWorkspaceView& current,
    const ProgramCatalog* /*catalog*/) {
    FermentationUiWorkspacePress result;
    if (page_ == FermentationUiPage::ValueEdit) {
        // `enabled` guarantees an in-range, allowed key.
        NumericEditInput input;
        if (target.row < 3U) {
            input = {NumericEditAction::Digit,
                     static_cast<std::uint8_t>(target.row * 3U + target.column +
                                               1U)};
        } else if (target.column == 0U) {
            input = {NumericEditAction::DecimalSeparator, 0U};
        } else if (target.column == 1U) {
            input = {NumericEditAction::Digit, 0U};
        } else {
            input = {NumericEditAction::Minus, 0U};
        }
        result.navigated = valueEdit_.apply(input);
        return result;
    }
    // ProgramSummary: `enabled` guarantees a summary.
    const auto& summary = *current.programSummary;
    if (target.column == 1U) {
        result.navigated =
            target.row == 0U ? pager_.moveUp() : pager_.moveDown();
        return result;
    }
    const auto field = summary.fields[current.pager.currentIndex + target.row];
    if (isNumericStartField(field)) {
        valueEditField_ = field;
        valueEdit_.reset(startValueText(field, summary));
        pageStack_.push_back(FermentationUiPage::ValueEdit);
        page_ = FermentationUiPage::ValueEdit;
        displayLanguageChangeFailed_ = false;
        result.navigated = true;
        return result;
    }
    cycleStartField(field, summary);
    result.navigated = true;
    return result;
}

// One explicit cycle step per tap for the non-numeric start fields. Only the
// next-run candidate changes; the stored program is never touched.
void FermentationTouchWorkspace::cycleStartField(
    FermentationUiStartField field,
    const FermentationUiProgramSummaryView& summary) {
    switch (field) {
        case FermentationUiStartField::Preheat:
            selectedCandidate_.preheatEnabled = !summary.preheat;
            break;
        case FermentationUiStartField::SensorMode:
            // Only structurally allowed alternatives to the preference's
            // default are offered; a mode equal to the default is no override.
            if (const auto next = nextSensorOverride(
                    summary.sensorPreference, summary.sensorModeOverride)) {
                selectedCandidate_.sensorMode = *next;
            }
            break;
        case FermentationUiStartField::CompletionMode: {
            constexpr std::uint8_t kModeCount = 4U;
            const auto next = static_cast<CompletionMode>(
                (static_cast<std::uint8_t>(summary.completionMode) + 1U) %
                kModeCount);
            selectedCandidate_.completionMode = next;
            // Values the new mode does not use are dropped from the candidate
            // (the shared override definition resets them in the copy).
            if (next == CompletionMode::FinishWithoutCooling) {
                selectedCandidate_.coolingTargetCelsius.reset();
                selectedCandidate_.holdDurationMinutes.reset();
            } else if (next != CompletionMode::CoolAndHoldForDuration) {
                selectedCandidate_.holdDurationMinutes.reset();
            }
            break;
        }
        case FermentationUiStartField::TargetTemperature:
        case FermentationUiStartField::Duration:
        case FermentationUiStartField::CoolingTarget:
        case FermentationUiStartField::HoldDuration:
            break;
    }
}

void FermentationTouchWorkspace::commitValueEdit() {
    // The view only enables the slot for a value the program validator
    // accepts for this field, so a parsed value is stored as-is.
    if (valueEdit_.apply({NumericEditAction::Commit, 0U}) &&
        valueEdit_.committedValue().has_value()) {
        setNumericStartOverride(selectedCandidate_, valueEditField_,
                                *valueEdit_.committedValue());
    }
    (void)goBack();
}

void FermentationTouchWorkspace::setPage(FermentationUiPage page) {
    markRenderRelevantChange();
    setCanonicalPageStack(page);
}

}  // namespace fermentation
