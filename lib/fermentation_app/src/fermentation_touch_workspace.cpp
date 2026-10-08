#include "fermentation_touch_workspace.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "configuration_limits.hpp"
#include "configuration_text.hpp"
#include "fermentation_ui_editing.hpp"
#include "fermentation_ui_text.hpp"
#include "program_limits.hpp"

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

// The persistent factory reset is offered (and therefore not listed as
// unavailable) only while the application reports the flow as available.
std::vector<FermentationUiSafeBootCapability> safeBootUnavailableCapabilities(
    bool factoryResetAvailable) {
    std::vector<FermentationUiSafeBootCapability> result;
    if (!factoryResetAvailable) {
        result.push_back(
            FermentationUiSafeBootCapability::PersistentFactoryReset);
    }
    result.push_back(FermentationUiSafeBootCapability::RawTouchRecovery);
    result.push_back(
        FermentationUiSafeBootCapability::NetworkProvisioningRecovery);
    result.push_back(FermentationUiSafeBootCapability::DiagnosticsExport);
    return result;
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
bool keypadKeyEnabled(std::uint8_t row, std::uint8_t column, bool wholeNumber,
                      const std::string& candidate) noexcept {
    if (row >= kFermentationUiKeypadRows ||
        column >= kFermentationUiKeypadColumns) {
        return false;
    }
    if (row == 3U && column != 1U) {
        if (wholeNumber) return false;
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

// Real manual run values are accepted with the canonical program limits (the
// same constants the manual-run validators use); the technical limits are not
// entered here.
bool manualNumericValueAccepted(FermentationUiStartField field,
                                double value) noexcept {
    if (!std::isfinite(value)) return false;
    switch (field) {
        case FermentationUiStartField::TargetTemperature:
            return value >=
                       program_limits::kMinimumFermentationTemperatureCelsius &&
                   value <=
                       program_limits::kMaximumFermentationTemperatureCelsius;
        case FermentationUiStartField::CoolingTarget:
            return value >= program_limits::kMinimumCoolingTargetCelsius &&
                   value <= program_limits::kMaximumCoolingTargetCelsius;
        case FermentationUiStartField::Duration:
            return value == std::floor(value) &&
                   value >=
                       program_limits::kMinimumFermentationDurationMinutes &&
                   value <= program_limits::kMaximumFermentationDurationMinutes;
        case FermentationUiStartField::HoldDuration:
            return value == std::floor(value) &&
                   value >= program_limits::kMinimumHoldDurationMinutes &&
                   value <= program_limits::kMaximumHoldDurationMinutes;
        case FermentationUiStartField::Preheat:
        case FermentationUiStartField::SensorMode:
        case FermentationUiStartField::CompletionMode:
            break;
    }
    return false;
}

// A cooling plan's single value is the plan target (the manual-run validator
// bounds it like a run target), shown as the cooling target.
FermentationUiStartField manualLimitField(
    FermentationUiManualDraftSlot slot,
    FermentationUiStartField field) noexcept {
    return (slot == FermentationUiManualDraftSlot::StopCooling ||
            slot == FermentationUiManualDraftSlot::CompletionCooling)
               ? FermentationUiStartField::TargetTemperature
               : field;
}

void setDraftNumeric(FermentationUiManualDraft& draft,
                     FermentationUiStartField field, double value) noexcept {
    switch (field) {
        case FermentationUiStartField::TargetTemperature:
            draft.targetTemperatureCelsius = value;
            break;
        case FermentationUiStartField::Duration:
            draft.durationMinutes = static_cast<std::uint32_t>(value);
            break;
        case FermentationUiStartField::CoolingTarget:
            draft.coolingTargetCelsius = value;
            break;
        case FermentationUiStartField::HoldDuration:
            draft.holdDurationMinutes = static_cast<std::uint32_t>(value);
            break;
        case FermentationUiStartField::Preheat:
        case FermentationUiStartField::SensorMode:
        case FermentationUiStartField::CompletionMode:
            break;
    }
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

// ---- S10: program editor and keyboard helpers
// --------------------------------

using EditField = FermentationUiProgramField;

const char* programFieldLabelKey(EditField field) noexcept {
    switch (field) {
        case EditField::Name:
            return "pf-name";
        case EditField::Notes:
            return "pf-notes";
        case EditField::TargetTemperature:
            return "label-target";
        case EditField::Duration:
            return "label-duration";
        case EditField::Preheat:
            return "label-preheat";
        case EditField::MaxProductWait:
            return "pf-wait";
        case EditField::SensorPreference:
            return "label-sensor";
        case EditField::FailurePolicy:
            return "pf-failure";
        case EditField::FallbackDelay:
            return "pf-delay";
        case EditField::ReturnStrategy:
            return "pf-return";
        case EditField::MaxTargetReach:
            return "pf-reach";
        case EditField::CompletionMode:
            return "label-completion";
        case EditField::CoolingTarget:
            return "label-cooling";
        case EditField::HoldDuration:
            return "label-hold";
    }
    return "pf-name";
}

// Title of the numeric edit page for a program field.
const char* programFieldTitleKey(EditField field) noexcept {
    switch (field) {
        case EditField::TargetTemperature:
            return "field-target";
        case EditField::Duration:
            return "field-duration";
        case EditField::MaxProductWait:
            return "pt-wait";
        case EditField::FallbackDelay:
            return "pt-delay";
        case EditField::MaxTargetReach:
            return "pt-reach";
        case EditField::CoolingTarget:
            return "field-cooling";
        case EditField::HoldDuration:
            return "field-hold";
        case EditField::Name:
        case EditField::Notes:
        case EditField::Preheat:
        case EditField::SensorPreference:
        case EditField::FailurePolicy:
        case EditField::ReturnStrategy:
        case EditField::CompletionMode:
            break;
    }
    return "program-edit";
}

bool isNumericProgramField(EditField field) noexcept {
    switch (field) {
        case EditField::TargetTemperature:
        case EditField::Duration:
        case EditField::MaxProductWait:
        case EditField::FallbackDelay:
        case EditField::MaxTargetReach:
        case EditField::CoolingTarget:
        case EditField::HoldDuration:
            return true;
        case EditField::Name:
        case EditField::Notes:
        case EditField::Preheat:
        case EditField::SensorPreference:
        case EditField::FailurePolicy:
        case EditField::ReturnStrategy:
        case EditField::CompletionMode:
            break;
    }
    return false;
}

bool isWholeNumberProgramField(EditField field) noexcept {
    return isNumericProgramField(field) &&
           field != EditField::TargetTemperature &&
           field != EditField::CoolingTarget;
}

FermentationUiValueUnit programFieldUnit(EditField field) noexcept {
    switch (field) {
        case EditField::TargetTemperature:
        case EditField::CoolingTarget:
            return FermentationUiValueUnit::Celsius;
        case EditField::FallbackDelay:
            return FermentationUiValueUnit::Seconds;
        default:
            return FermentationUiValueUnit::Minutes;
    }
}

// Validation field of each numeric program field: the program validator names
// the field of every error, so the commit check reuses it (no parallel limits).
const char* programNumericFieldName(EditField field) noexcept {
    switch (field) {
        case EditField::TargetTemperature:
            return "defaults.fermentation_temperature_c";
        case EditField::Duration:
            return "defaults.fermentation_duration_min";
        case EditField::MaxProductWait:
            return "defaults.max_product_wait_min";
        case EditField::FallbackDelay:
            return "defaults.product_sensor_failure.fallback_delay_s";
        case EditField::MaxTargetReach:
            return "defaults.max_target_reach_min";
        case EditField::CoolingTarget:
            return "defaults.completion.cooling_target_c";
        case EditField::HoldDuration:
            return "defaults.completion.hold_duration_min";
        default:
            break;
    }
    return "";
}

std::optional<double> programNumericValue(const ProgramDefinition& program,
                                          EditField field) {
    const FermentationStage stage = program.fermentationStages.empty()
                                        ? FermentationStage{}
                                        : program.fermentationStages.front();
    const auto whole = [](const std::optional<std::uint32_t>& value) {
        return value.has_value() ? std::optional<double>{*value}
                                 : std::optional<double>{};
    };
    switch (field) {
        case EditField::TargetTemperature:
            return stage.targetTemperatureCelsius;
        case EditField::Duration:
            return whole(stage.durationMinutes);
        case EditField::MaxProductWait:
            return whole(program.maximumProductWaitMinutes);
        case EditField::FallbackDelay:
            return whole(program.productSensorFailure.fallbackDelaySeconds);
        case EditField::MaxTargetReach:
            return whole(program.maximumTargetReachMinutes);
        case EditField::CoolingTarget:
            return program.completion.coolingTargetCelsius;
        case EditField::HoldDuration:
            return whole(program.completion.holdDurationMinutes);
        default:
            break;
    }
    return std::nullopt;
}

void setProgramNumeric(ProgramDefinition& program, EditField field,
                       double value) {
    const auto whole = static_cast<std::uint32_t>(value);
    switch (field) {
        case EditField::TargetTemperature:
            if (!program.fermentationStages.empty())
                program.fermentationStages.front().targetTemperatureCelsius =
                    value;
            break;
        case EditField::Duration:
            if (!program.fermentationStages.empty())
                program.fermentationStages.front().durationMinutes = whole;
            break;
        case EditField::MaxProductWait:
            program.maximumProductWaitMinutes = whole;
            break;
        case EditField::FallbackDelay:
            program.productSensorFailure.fallbackDelaySeconds = whole;
            break;
        case EditField::MaxTargetReach:
            program.maximumTargetReachMinutes = whole;
            break;
        case EditField::CoolingTarget:
            program.completion.coolingTargetCelsius = value;
            break;
        case EditField::HoldDuration:
            program.completion.holdDurationMinutes = whole;
            break;
        default:
            break;
    }
}

std::string programNumericText(EditField field, double value) {
    char buffer[24];
    if (isWholeNumberProgramField(field)) {
        std::snprintf(buffer, sizeof(buffer), "%u",
                      static_cast<unsigned>(value));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.1f", value);
    }
    return buffer;
}

std::string programNumericDisplay(EditField field,
                                  const std::optional<double>& value) {
    if (!value.has_value()) return "--";
    auto text = programNumericText(field, *value);
    switch (programFieldUnit(field)) {
        case FermentationUiValueUnit::Celsius:
            text += " C";
            break;
        case FermentationUiValueUnit::Minutes:
            text += " min";
            break;
        case FermentationUiValueUnit::Seconds:
            text += " s";
            break;
    }
    return text;
}

// A value is accepted for the field when the existing program validator
// reports no error for exactly that field on the candidate with the value
// applied; errors of other fields (a dependent value still missing) do not
// block this commit but keep `valid` false for the save.
bool programNumericValueAccepted(const ProgramDocument& candidate,
                                 EditField field, double value) {
    constexpr double kWholeNumberLimit = 4'000'000'000.0;
    if (!std::isfinite(value)) return false;
    if (isWholeNumberProgramField(field) &&
        (value < 0.0 || value > kWholeNumberLimit ||
         value != std::floor(value))) {
        return false;
    }
    auto copy = candidate;
    setProgramNumeric(copy.program, field, value);
    const std::string_view name = programNumericFieldName(field);
    const auto result =
        validateProgram(copy, ValidationPurpose::CatalogTemplate);
    return std::none_of(
        result.errors.begin(), result.errors.end(),
        [name](const ValidationError& error) { return name == error.field; });
}

device_platform::TextKey failurePolicyKey(ProductSensorFailurePolicy policy) {
    switch (policy) {
        case ProductSensorFailurePolicy::FallbackToAirAfterTimeout:
            return fermentationTextKey("policy-fallback");
        case ProductSensorFailurePolicy::WaitForUser:
            return fermentationTextKey("policy-wait");
        case ProductSensorFailurePolicy::StopToSafeState:
            return fermentationTextKey("policy-stop");
    }
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey returnStrategyKey(ReturnStrategy strategy) {
    switch (strategy) {
        case ReturnStrategy::RemainOnAirUntilEnd:
            return fermentationTextKey("return-air");
        case ReturnStrategy::ManualReturnToProduct:
            return fermentationTextKey("return-manual");
        case ReturnStrategy::AutomaticValidatedReturnToProduct:
            return fermentationTextKey("return-auto");
    }
    return fermentationTextKey("message-unknown");
}

// Which rows exist for the program's current values: the rows of values the
// program model's own predicates mark as used (the validator and the editor
// share them, so the cross-field rules have one definition).
bool programFieldVisible(const ProgramDefinition& program,
                         EditField field) noexcept {
    switch (field) {
        case EditField::MaxProductWait:
            return programUsesProductWait(program);
        case EditField::FallbackDelay:
            return programUsesFallbackDelay(program);
        case EditField::FailurePolicy:
        case EditField::ReturnStrategy:
            // AirOnly has exactly one valid combination (6.13).
            return !programHasFixedSensorFailure(program);
        case EditField::CoolingTarget:
            return programUsesCoolingTarget(program);
        case EditField::HoldDuration:
            return programUsesHoldDuration(program);
        default:
            break;
    }
    return true;
}

// One cycle step of a non-numeric program field. Values the new setting makes
// unexpected are dropped by the program model's own rule
// (clearUnexpectedProgramValues, which also applies the AirOnly
// normalization).
void cycleProgramField(ProgramDefinition& program, EditField field) {
    constexpr std::uint8_t kPreferences = 4U;
    constexpr std::uint8_t kPolicies = 3U;
    constexpr std::uint8_t kStrategies = 3U;
    constexpr std::uint8_t kModes = 4U;
    switch (field) {
        case EditField::Preheat:
            program.preheat = !program.preheat;
            break;
        case EditField::SensorPreference:
            program.sensorPreference = static_cast<SensorPreference>(
                (static_cast<std::uint8_t>(program.sensorPreference) + 1U) %
                kPreferences);
            break;
        case EditField::FailurePolicy:
            program.productSensorFailure.policy =
                static_cast<ProductSensorFailurePolicy>(
                    (static_cast<std::uint8_t>(
                         program.productSensorFailure.policy) +
                     1U) %
                    kPolicies);
            break;
        case EditField::ReturnStrategy:
            program.productSensorFailure.returnStrategy =
                static_cast<ReturnStrategy>(
                    (static_cast<std::uint8_t>(
                         program.productSensorFailure.returnStrategy) +
                     1U) %
                    kStrategies);
            break;
        case EditField::CompletionMode:
            program.completion.mode = static_cast<CompletionMode>(
                (static_cast<std::uint8_t>(program.completion.mode) + 1U) %
                kModes);
            break;
        default:
            return;
    }
    clearUnexpectedProgramValues(program);
}

// ---- keyboard
// ----------------------------------------------------------------

// Characters of rows 0-2 per mode (10 columns each; '\0' = no key). Only ASCII
// is offered (the standard font), longer text such as an existing multi-byte
// name stays editable through Backspace/Clear.
constexpr std::array<std::array<const char*, 3U>, 4U> kKeyboardLayout{{
    {"abcdefghij", "klmnopqrst", "uvwxyz\0\0\0\0"},
    {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ\0\0\0\0"},
    {"1234567890", "+*/=%#@&_$", "()[]:;,!?'"},
    {".,:;!?'\"()", "[]{}<>+=*/", "#%&@_^~|$\0"},
}};

bool textEditCommitAccepted(FermentationUiTextTarget target,
                            const std::string& value) {
    switch (target) {
        case FermentationUiTextTarget::DeviceName:
        case FermentationUiTextTarget::ProgramName:
            return validateVisibleName(value) ==
                   ConfigurationTextStatus::Success;
        case FermentationUiTextTarget::ProgramNotes:
            return validateProgramNotes(value) ==
                   ConfigurationTextStatus::Success;
    }
    return false;
}

std::size_t textEditByteLimit(FermentationUiTextTarget target) noexcept {
    return target == FermentationUiTextTarget::ProgramNotes
               ? configuration_limits::kMaximumNotesBytes
               : configuration_limits::kMaximumVisibleNameBytes;
}

}  // namespace

FermentationUiKeyboardKey fermentationUiKeyboardKeyAt(
    TextEditMode mode, std::uint8_t row, std::uint8_t column) noexcept {
    if (row >= kFermentationUiKeyboardRows ||
        column >= kFermentationUiKeyboardColumns) {
        return {};
    }
    if (row == 3U) {
        if (column < 2U) return {FermentationUiKeyboardKeyKind::Clear, '\0'};
        if (column < 8U) return {FermentationUiKeyboardKeyKind::Character, ' '};
        return {FermentationUiKeyboardKeyKind::Character,
                column == 8U ? '-' : '.'};
    }
    const char character =
        kKeyboardLayout[static_cast<std::size_t>(mode)][row][column];
    if (character == '\0') return {};
    return {FermentationUiKeyboardKeyKind::Character, character};
}

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
        case FermentationUiWorkspaceSlotAction::FactoryResetCancel:
        case FermentationUiWorkspaceSlotAction::FactoryResetDismiss:
            return true;
        case FermentationUiWorkspaceSlotAction::FactoryResetBegin:
        case FermentationUiWorkspaceSlotAction::FactoryResetAcknowledge:
        case FermentationUiWorkspaceSlotAction::FactoryResetHold:
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
        case FermentationUiWorkspaceSlotAction::NavigateSettings:
        case FermentationUiWorkspaceSlotAction::TextEditCancel:
        case FermentationUiWorkspaceSlotAction::TextEditMode:
        case FermentationUiWorkspaceSlotAction::TextEditBackspace:
        case FermentationUiWorkspaceSlotAction::TextEditCommit:
        case FermentationUiWorkspaceSlotAction::DiscardProgramEdit:
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
            route.push_back(key("settings"));
            route.push_back(key("service"));
            break;
        case FermentationUiPage::Pin:
            route.push_back(key("settings"));
            route.push_back(key("service"));
            route.push_back(key("pin"));
            break;
        case FermentationUiPage::Settings:
            route.push_back(key("settings"));
            break;
        case FermentationUiPage::TextEdit:
            route.push_back(key("settings"));
            route.push_back(key("edit"));
            break;
        case FermentationUiPage::Recovery:
            route.push_back(key("recovery"));
            break;
        case FermentationUiPage::FactoryReset:
            route.push_back(key("factory-reset"));
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
        case FermentationUiPage::Settings:
            pageStack_.push_back(FermentationUiPage::Settings);
            break;
        case FermentationUiPage::TextEdit:
            pageStack_.insert(pageStack_.end(), {FermentationUiPage::Settings,
                                                 FermentationUiPage::TextEdit});
            break;
        case FermentationUiPage::Service:
            pageStack_.insert(pageStack_.end(), {FermentationUiPage::Settings,
                                                 FermentationUiPage::Service});
            break;
        case FermentationUiPage::Pin:
            pageStack_.insert(pageStack_.end(), {FermentationUiPage::Settings,
                                                 FermentationUiPage::Service,
                                                 FermentationUiPage::Pin});
            break;
        case FermentationUiPage::Recovery:
            pageStack_.push_back(FermentationUiPage::Recovery);
            break;
        case FermentationUiPage::FactoryReset:
            pageStack_.push_back(FermentationUiPage::FactoryReset);
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
            // Service moved under Settings (D14); its reason is shown there.
            setSlot(view, 3U, "settings",
                    FermentationUiWorkspaceSlotAction::NavigateSettings);
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
                if (snapshot.home.processState == ProcessState::SafeBoot) {
                    // SAFE_BOOT: own local entry to the full factory reset,
                    // independent of the locked service area (O-R1 = B+).
                    setSlot(
                        view, 0U, "factory-reset",
                        FermentationUiWorkspaceSlotAction::FactoryResetBegin,
                        snapshot.factoryReset.available);
                } else {
                    setSlot(
                        view, 0U, "recovery",
                        FermentationUiWorkspaceSlotAction::NavigateRecovery);
                }
                if (snapshot.home.processState == ProcessState::SafeBoot)
                    view.unavailableCapabilities =
                        safeBootUnavailableCapabilities(
                            snapshot.factoryReset.available);
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
            if (snapshot.home.processState == ProcessState::SafeBoot) {
                setSlot(view, 0U, "factory-reset",
                        FermentationUiWorkspaceSlotAction::FactoryResetBegin,
                        snapshot.factoryReset.available);
            } else {
                setSlot(view, 0U, "recovery",
                        FermentationUiWorkspaceSlotAction::NavigateRecovery,
                        snapshot.home.processState ==
                            ProcessState::RecoveryEvaluation);
            }
            setSlot(view, 1U, "programs",
                    FermentationUiWorkspaceSlotAction::NavigateProgramList,
                    snapshot.home.processState != ProcessState::SafeBoot);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "service",
                    FermentationUiWorkspaceSlotAction::NavigateService, false);
            view.blockedReason = key("restricted");
            if (snapshot.home.processState == ProcessState::SafeBoot)
                view.unavailableCapabilities = safeBootUnavailableCapabilities(
                    snapshot.factoryReset.available);
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
    if (page_ == FermentationUiPage::TextEdit && pageStack_.size() >= 2U) {
        // The keyboard is shared; its route is the caller's route (the page
        // below it on the existing stack) plus the edit step.
        view.route.segments = routeForPage(pageStack_[pageStack_.size() - 2U]);
        view.route.segments.push_back(key("edit"));
    }
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
            view.title = key(valueEditProgramField_.has_value()
                                 ? programFieldTitleKey(*valueEditProgramField_)
                                 : valueEditTitleKey(valueEditField_));
            view.pager.currentIndex = pager_.currentIndex;
            FermentationUiValueEditView edit;
            edit.field = valueEditField_;
            edit.candidate = valueEdit_.candidate();
            if (valueEditProgramField_.has_value()) {
                edit.unit = programFieldUnit(*valueEditProgramField_);
                edit.wholeNumber =
                    isWholeNumberProgramField(*valueEditProgramField_);
            } else {
                edit.unit =
                    (valueEditField_ ==
                         FermentationUiStartField::TargetTemperature ||
                     valueEditField_ == FermentationUiStartField::CoolingTarget)
                        ? FermentationUiValueUnit::Celsius
                        : FermentationUiValueUnit::Minutes;
                edit.wholeNumber = isWholeNumberStartField(valueEditField_);
            }
            if (valueEditProgramField_.has_value()) {
                // Program field: the candidate with the value applied must
                // pass the existing program validator for exactly this field.
                auto probe = valueEdit_;
                if (programEditCandidate_.has_value() &&
                    probe.apply({NumericEditAction::Commit, 0U}) &&
                    probe.committedValue().has_value()) {
                    edit.commitValid = programNumericValueAccepted(
                        *programEditCandidate_, *valueEditProgramField_,
                        *probe.committedValue());
                }
            } else if (valueEditSlot_ !=
                       FermentationUiManualDraftSlot::StartCandidate) {
                // Manual run value: the canonical program limits decide.
                auto probe = valueEdit_;
                if (probe.apply({NumericEditAction::Commit, 0U}) &&
                    probe.committedValue().has_value()) {
                    edit.commitValid = manualNumericValueAccepted(
                        manualLimitField(valueEditSlot_, valueEditField_),
                        *probe.committedValue());
                }
            } else if (catalog != nullptr && selectedProgramId_.has_value()) {
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
            // A dirty editor cannot be left through the navigation exits
            // (ConfirmDiscard): the explicit discard is the confirmation.
            if (programEditDirty_) {
                setSlot(view, 0U, "discard",
                        FermentationUiWorkspaceSlotAction::DiscardProgramEdit);
            } else {
                setSlot(view, 0U, "back",
                        FermentationUiWorkspaceSlotAction::NavigateBack);
            }
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
            view.programEdit = makeProgramEditView(catalog);
            view.pager.itemCount = view.programEdit->rowCount;
            view.pager.currentIndex =
                view.programEdit->rowCount == 0U
                    ? 0U
                    : std::min(pager_.currentIndex,
                               view.programEdit->rowCount - 1U);
            // Edit saves a valid candidate; Copy and New save with an optional
            // valid name. The owner validates and applies again.
            setSlot(view, 3U, "save",
                    FermentationUiWorkspaceSlotAction::SaveProgram,
                    view.programEdit->valid &&
                        (programEditOperation_ !=
                             FermentationUiProgramEditOperation::Edit ||
                         programEditCandidate_.has_value()));
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
            applyManualFieldView(view,
                                 FermentationUiManualDraftSlot::ManualHolding);
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "cancel",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 2U, "confirm",
                    FermentationUiWorkspaceSlotAction::StartManualHolding,
                    false);
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::ManualTimed:
            view.title = key("manual-timed");
            applyManualFieldView(view,
                                 FermentationUiManualDraftSlot::ManualTimed);
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "cancel",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 2U, "confirm",
                    FermentationUiWorkspaceSlotAction::StartManualTimed, false);
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
            applyManualFieldView(view,
                                 FermentationUiManualDraftSlot::StopCooling);
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "stop-turn-off",
                    FermentationUiWorkspaceSlotAction::StopTurnOff);
            setSlot(view, 2U, "stop-and-cool",
                    FermentationUiWorkspaceSlotAction::StopAndCool, false);
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
            applyManualFieldView(
                view, FermentationUiManualDraftSlot::CompletionCooling);
            setSlot(view, 3U, "cool-now",
                    FermentationUiWorkspaceSlotAction::CompleteAndCool, false);
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
            // The PIN page itself is reachable while the service area is
            // locked: it carries the PIN-independent recovery entry.
            setSlot(view, 1U, "pin",
                    FermentationUiWorkspaceSlotAction::NavigatePin);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "recovery",
                    FermentationUiWorkspaceSlotAction::NavigateRecovery,
                    snapshot.service.available);
            break;
        case FermentationUiPage::Pin:
            view.title = key("pin");
            // "PIN forgotten?" needs no PIN entry (O-R1 = B+); it starts the
            // full factory reset flow, never a PIN-only reset.
            setSlot(view, 1U, "cancel",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 2U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            setSlot(view, 3U, "forgot-pin",
                    FermentationUiWorkspaceSlotAction::FactoryResetBegin,
                    snapshot.factoryReset.available);
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
                view.unavailableCapabilities = safeBootUnavailableCapabilities(
                    snapshot.factoryReset.available);
            }
            break;
        }
        case FermentationUiPage::FactoryReset: {
            view.title = key("factory-reset");
            const auto& reset = snapshot.factoryReset;
            view.factoryReset = FermentationUiFactoryResetPageView{
                reset.stage, reset.outcome, reset.available,
                reset.holdProgressTenths};
            using Action = FermentationUiWorkspaceSlotAction;
            switch (reset.stage) {
                case FactoryResetStage::Idle:
                    // The flow is not running (begin refused or flow ended).
                    setSlot(view, 0U, "back", Action::NavigateBack);
                    break;
                case FactoryResetStage::PinRequired:
                case FactoryResetStage::Warning:
                    setSlot(view, 0U, "cancel", Action::FactoryResetCancel);
                    setSlot(view, 2U, "continue",
                            Action::FactoryResetAcknowledge);
                    break;
                case FactoryResetStage::Confirm:
                    setSlot(view, 0U, "cancel", Action::FactoryResetCancel);
                    setSlot(view, 2U, "confirm",
                            Action::FactoryResetAcknowledge);
                    break;
                case FactoryResetStage::Hold:
                    setSlot(view, 0U, "cancel", Action::FactoryResetCancel);
                    setSlot(view, kFermentationUiFactoryResetHoldSlot,
                            "factory-reset-hold", Action::FactoryResetHold);
                    break;
                case FactoryResetStage::Executing:
                    // Not cancellable while the core runs.
                    break;
                case FactoryResetStage::Finished:
                    setSlot(view, 0U, "ok", Action::FactoryResetDismiss);
                    break;
            }
            break;
        }
        case FermentationUiPage::Settings:
            view.title = key("settings-page");
            view.settings = makeSettingsView(snapshot);
            view.pager.itemCount = kFermentationUiSettingsRowCount;
            view.pager.currentIndex = pager_.currentIndex;
            if (deviceNameChangeFailed_)
                view.blockedReason = key("device-name-change-failed");
            setSlot(view, 0U, "back",
                    FermentationUiWorkspaceSlotAction::NavigateBack);
            setSlot(view, 1U, "up",
                    FermentationUiWorkspaceSlotAction::MovePagerUp,
                    view.pager.canMoveUp());
            setSlot(view, 2U, "down",
                    FermentationUiWorkspaceSlotAction::MovePagerDown,
                    view.pager.canMoveDown());
            setSlot(view, 3U, "status",
                    FermentationUiWorkspaceSlotAction::NavigateStatus);
            break;
        case FermentationUiPage::TextEdit: {
            switch (textTarget_) {
                case FermentationUiTextTarget::DeviceName:
                    view.title = key("device-name");
                    break;
                case FermentationUiTextTarget::ProgramName:
                    view.title = key("program-name");
                    break;
                case FermentationUiTextTarget::ProgramNotes:
                    view.title = key("program-notes");
                    break;
            }
            FermentationUiTextEditView edit;
            edit.target = textTarget_;
            edit.candidate = textEdit_.candidate();
            edit.mode = textEdit_.mode();
            edit.commitValid =
                textEditCommitAccepted(textTarget_, edit.candidate);
            edit.full = edit.candidate.size() >= textEditByteLimit(textTarget_);
            const char* modeLabel = "kbd-lower";
            switch (edit.mode) {
                case TextEditMode::Lowercase:
                    modeLabel = "kbd-lower";
                    break;
                case TextEditMode::Uppercase:
                    modeLabel = "kbd-upper";
                    break;
                case TextEditMode::Digits:
                    modeLabel = "kbd-digits";
                    break;
                case TextEditMode::Symbols:
                    modeLabel = "kbd-symbols";
                    break;
            }
            view.textEdit = std::move(edit);
            setSlot(view, 0U, "cancel",
                    FermentationUiWorkspaceSlotAction::TextEditCancel);
            setSlot(view, 1U, modeLabel,
                    FermentationUiWorkspaceSlotAction::TextEditMode);
            setSlot(view, 2U, "backspace",
                    FermentationUiWorkspaceSlotAction::TextEditBackspace,
                    !textEdit_.candidate().empty());
            setSlot(view, 3U, "ok",
                    FermentationUiWorkspaceSlotAction::TextEditCommit,
                    view.textEdit->commitValid);
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
            // The provisional web access slot (#170) moved to the settings
            // page (D13).
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
        case FermentationUiWorkspaceSlotAction::NavigateSettings:
            destination = FermentationUiPage::Settings;
            break;
        case FermentationUiWorkspaceSlotAction::FactoryResetBegin:
            destination = FermentationUiPage::FactoryReset;
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
    const auto left = page_;
    if (pageStack_.size() == 2U) {
        pageStack_.resize(1U);
        page_ = FermentationUiPage::Home;
    } else {
        pageStack_.pop_back();
        page_ = pageStack_.back();
    }
    displayLanguageChangeFailed_ = false;
    // The value and keyboard pages are sub-steps of the program editor; only
    // leaving the editor itself discards its candidate and name.
    if (left != FermentationUiPage::ValueEdit &&
        left != FermentationUiPage::TextEdit) {
        programEditDirty_ = false;
        if (left == FermentationUiPage::ProgramEdit) {
            // Leaving the editor discards its edits.
            programEditName_.reset();
            programEditCandidate_.reset();
        }
    }
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
                    programEditCandidate_,
                    programEditOperation_ ==
                            FermentationUiProgramEditOperation::Edit
                        ? std::nullopt
                        : programEditName_,
                    true};
                // The editor stays dirty until the owner accepted the request
                // (noteProgramEditOutcome): a refused save keeps the candidate
                // and the discard protection.
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
        case FermentationUiWorkspaceSlotAction::FactoryResetBegin:
            result.navigated = navigate(action);
            result.factoryReset = FermentationUiFactoryResetCommand{
                FermentationUiFactoryResetCommand::Step::Begin};
            break;
        case FermentationUiWorkspaceSlotAction::FactoryResetAcknowledge:
            result.factoryReset = FermentationUiFactoryResetCommand{
                FermentationUiFactoryResetCommand::Step::Acknowledge};
            break;
        case FermentationUiWorkspaceSlotAction::FactoryResetCancel:
            result.factoryReset = FermentationUiFactoryResetCommand{
                FermentationUiFactoryResetCommand::Step::Cancel};
            result.navigated = goBack();
            break;
        case FermentationUiWorkspaceSlotAction::FactoryResetDismiss:
            result.factoryReset = FermentationUiFactoryResetCommand{
                FermentationUiFactoryResetCommand::Step::Dismiss};
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateHome);
            break;
        case FermentationUiWorkspaceSlotAction::FactoryResetHold:
            // Sustained contact only; the touch dispatcher evaluates it.
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
        case FermentationUiWorkspaceSlotAction::StartManualTimed:
            // O5: the technical run limits of a manual run have no owner yet,
            // so the local UI builds no manual start payload (the slot is
            // disabled with a visible reason; the Application refuses it too).
            break;
        case FermentationUiWorkspaceSlotAction::StopTurnOff:
            result.action = FermentationUiEnvelopePayload{
                makeStopIntent(StopOption::AbortAndTurnOff)};
            break;
        case FermentationUiWorkspaceSlotAction::StopAndCool:
        case FermentationUiWorkspaceSlotAction::CompleteAndCool:
            // O5: a cooling plan is a manual run plan; same fail-closed rule.
            break;
        case FermentationUiWorkspaceSlotAction::Complete:
            result.action =
                FermentationUiEnvelopePayload{makeCompletionIntent(false)};
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
        case FermentationUiWorkspaceSlotAction::TextEditCancel:
            result.navigated = goBack();
            break;
        case FermentationUiWorkspaceSlotAction::DiscardProgramEdit:
            // The user confirmed the discard: the edits go, the stored
            // program was never touched.
            programEditDirty_ = false;
            result.navigated = goBack();
            break;
        case FermentationUiWorkspaceSlotAction::TextEditMode:
            result.navigated = textEdit_.apply({TextEditAction::Mode, ' '});
            break;
        case FermentationUiWorkspaceSlotAction::TextEditBackspace:
            result.navigated =
                textEdit_.apply({TextEditAction::Backspace, ' '});
            break;
        case FermentationUiWorkspaceSlotAction::TextEditCommit:
            result = commitTextEdit(snapshot, current);
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
    const auto pageBefore = page_;
    auto result = pressImpl(snapshot, target, catalog);
    // Leaving the factory reset page by any navigation (back, home, header)
    // ends the flow: a visible flow must never stay armed behind another
    // page. A finished flow is acknowledged instead.
    if (pageBefore == FermentationUiPage::FactoryReset &&
        page_ != FermentationUiPage::FactoryReset &&
        !result.factoryReset.has_value()) {
        result.factoryReset = FermentationUiFactoryResetCommand{
            snapshot.factoryReset.stage == FactoryResetStage::Finished
                ? FermentationUiFactoryResetCommand::Step::Dismiss
                : FermentationUiFactoryResetCommand::Step::Cancel};
    }
    return result;
}

FermentationUiWorkspacePress FermentationTouchWorkspace::pressImpl(
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
        if (page_ != FermentationUiPage::ValueEdit &&
            current.programSummary.has_value()) {
            // A field list (program summary or manual page): rows are the
            // visible window of the fields (column 0, only while editable,
            // after the page's first-row offset); column 1 holds the two
            // pager buttons when the list needs them.
            const auto& summary = current.programSummary;
            const auto first = current.pager.currentIndex;
            const bool rowHit =
                target.column == 0U && summary->editable &&
                target.row >= summary->rowOffset &&
                static_cast<std::size_t>(target.row - summary->rowOffset) <
                    kFermentationUiListVisibleRows &&
                first + (target.row - summary->rowOffset) < summary->fieldCount;
            enabled =
                (rowHit &&
                 (summary->manual ||
                  summary->fields[first + (target.row - summary->rowOffset)] !=
                      FermentationUiStartField::SensorMode ||
                  nextSensorOverride(summary->sensorPreference, std::nullopt)
                      .has_value())) ||
                (summary->pagerButtons &&
                 ((target.column == 1U && target.row == 0U &&
                   current.pager.canMoveUp()) ||
                  (target.column == 1U && target.row == 1U &&
                   current.pager.canMoveDown())));
        } else if (page_ == FermentationUiPage::ValueEdit) {
            enabled = current.valueEdit.has_value() &&
                      keypadKeyEnabled(target.row, target.column,
                                       current.valueEdit->wholeNumber,
                                       current.valueEdit->candidate);
        } else if (page_ == FermentationUiPage::TextEdit) {
            // Keyboard: an existing key of the current mode; Clear needs text,
            // a character needs room under the owning byte limit.
            const auto key = fermentationUiKeyboardKeyAt(
                textEdit_.mode(), target.row, target.column);
            enabled = current.textEdit.has_value() &&
                      ((key.kind == FermentationUiKeyboardKeyKind::Clear &&
                        !current.textEdit->candidate.empty()) ||
                       (key.kind == FermentationUiKeyboardKeyKind::Character &&
                        !current.textEdit->full));
        } else if (page_ == FermentationUiPage::ProgramEdit &&
                   current.programEdit.has_value()) {
            // Program editor rows (column 0) and pager buttons (column 1).
            const auto& edit = *current.programEdit;
            const auto first = current.pager.currentIndex;
            enabled = (target.column == 0U &&
                       target.row < kFermentationUiListVisibleRows &&
                       first + target.row < edit.rowCount) ||
                      (edit.rowCount > kFermentationUiListVisibleRows &&
                       ((target.column == 1U && target.row == 0U &&
                         current.pager.canMoveUp()) ||
                        (target.column == 1U && target.row == 1U &&
                         current.pager.canMoveDown())));
        } else if (page_ == FermentationUiPage::Settings &&
                   current.settings.has_value()) {
            const auto index = current.pager.currentIndex + target.row;
            enabled = target.column == 0U &&
                      target.row < kFermentationUiListVisibleRows &&
                      index < kFermentationUiSettingsRowCount;
            if (enabled) {
                const auto row = static_cast<FermentationUiSettingsRow>(index);
                if (row == FermentationUiSettingsRow::DeviceName)
                    enabled = current.settings->deviceNameEditable;
                else if (row == FermentationUiSettingsRow::Service)
                    enabled = current.settings->serviceAvailable;
            }
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
            if (page_ == FermentationUiPage::ValueEdit ||
                current.programSummary.has_value()) {
                auto pressed =
                    pressContentCell(snapshot, target, current, catalog);
                pressed.interaction = result.interaction;
                return pressed;
            }
            if (page_ == FermentationUiPage::TextEdit) {
                auto pressed = pressKeyboardCell(target);
                pressed.interaction = result.interaction;
                return pressed;
            }
            if (page_ == FermentationUiPage::ProgramEdit) {
                auto pressed = pressProgramEditCell(current, target, catalog);
                pressed.interaction = result.interaction;
                return pressed;
            }
            if (page_ == FermentationUiPage::Settings) {
                auto pressed = pressSettingsRow(
                    snapshot, current.pager.currentIndex + target.row);
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
                case FermentationUiPage::TextEdit:
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
            result.factoryReset = pressed.factoryReset;
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
    // A field list (program summary or manual page): `enabled` guarantees a
    // summary.
    const auto& summary = *current.programSummary;
    if (target.column == 1U) {
        result.navigated =
            target.row == 0U ? pager_.moveUp() : pager_.moveDown();
        return result;
    }
    const auto field = summary.fields[current.pager.currentIndex +
                                      (target.row - summary.rowOffset)];
    const auto manualSlot = manualSlotForPage(page_);
    if (isNumericStartField(field)) {
        valueEditField_ = field;
        valueEditProgramField_.reset();
        valueEditSlot_ =
            manualSlot.value_or(FermentationUiManualDraftSlot::StartCandidate);
        valueEdit_.reset(startValueText(field, summary));
        pageStack_.push_back(FermentationUiPage::ValueEdit);
        page_ = FermentationUiPage::ValueEdit;
        displayLanguageChangeFailed_ = false;
        result.navigated = true;
        return result;
    }
    if (manualSlot.has_value()) {
        cycleManualField(*manualSlot, field, summary);
    } else {
        cycleStartField(field, summary);
    }
    result.navigated = true;
    return result;
}

// One cycle step per tap for the non-numeric manual fields: preheat toggles,
// the sensor chooses between the two real modes (no stored preference here),
// the completion mode walks the four modes and drops values it no longer
// uses.
void FermentationTouchWorkspace::cycleManualField(
    FermentationUiManualDraftSlot slot, FermentationUiStartField field,
    const FermentationUiProgramSummaryView& summary) {
    auto& draft = manualDrafts_[static_cast<std::size_t>(slot)];
    switch (field) {
        case FermentationUiStartField::Preheat:
            draft.preheat = !summary.preheat;
            break;
        case FermentationUiStartField::SensorMode:
            draft.sensorMode = summary.sensorMode == RunSensorMode::Air
                                   ? RunSensorMode::Product
                                   : RunSensorMode::Air;
            break;
        case FermentationUiStartField::CompletionMode: {
            constexpr std::uint8_t kModeCount = 4U;
            const auto next = static_cast<CompletionMode>(
                (static_cast<std::uint8_t>(summary.completionMode) + 1U) %
                kModeCount);
            draft.completionMode = next;
            if (next == CompletionMode::FinishWithoutCooling) {
                draft.coolingTargetCelsius.reset();
                draft.holdDurationMinutes.reset();
            } else if (next != CompletionMode::CoolAndHoldForDuration) {
                draft.holdDurationMinutes.reset();
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
        if (valueEditProgramField_.has_value()) {
            // Program editor: the candidate copy takes the value.
            if (programEditCandidate_.has_value()) {
                setProgramNumeric(programEditCandidate_->program,
                                  *valueEditProgramField_,
                                  *valueEdit_.committedValue());
                programEditDirty_ = true;
            }
        } else if (valueEditSlot_ ==
                   FermentationUiManualDraftSlot::StartCandidate) {
            setNumericStartOverride(selectedCandidate_, valueEditField_,
                                    *valueEdit_.committedValue());
        } else {
            setDraftNumeric(
                manualDrafts_[static_cast<std::size_t>(valueEditSlot_)],
                manualLimitField(valueEditSlot_, valueEditField_),
                *valueEdit_.committedValue());
        }
    }
    (void)goBack();
}

std::optional<FermentationUiManualDraftSlot>
FermentationTouchWorkspace::manualSlotForPage(
    FermentationUiPage page) noexcept {
    switch (page) {
        case FermentationUiPage::ManualHolding:
            return FermentationUiManualDraftSlot::ManualHolding;
        case FermentationUiPage::ManualTimed:
            return FermentationUiManualDraftSlot::ManualTimed;
        case FermentationUiPage::StopDialog:
            return FermentationUiManualDraftSlot::StopCooling;
        case FermentationUiPage::Completion:
            return FermentationUiManualDraftSlot::CompletionCooling;
        default:
            return std::nullopt;
    }
}

// Field list of a manual page: the values the user entered (draft), else the
// values already staged for the run; only real run values, never a default
// for a missing number.
FermentationUiProgramSummaryView
FermentationTouchWorkspace::makeManualFieldView(
    FermentationUiManualDraftSlot slot) const {
    const auto& draft = manualDrafts_[static_cast<std::size_t>(slot)];
    FermentationUiProgramSummaryView view;
    view.manual = true;
    view.editable = true;
    using Field = FermentationUiStartField;
    const auto add = [&view](Field field) {
        view.fields[view.fieldCount++] = field;
    };
    switch (slot) {
        case FermentationUiManualDraftSlot::ManualHolding: {
            const auto* base =
                manualHoldingValues_ ? &*manualHoldingValues_ : nullptr;
            view.targetTemperatureCelsius =
                draft.targetTemperatureCelsius.has_value()
                    ? draft.targetTemperatureCelsius
                : base ? std::optional<double>{base->targetTemperatureCelsius}
                       : std::nullopt;
            view.preheat = draft.preheat.value_or(base && base->preheatEnabled);
            view.sensorMode = draft.sensorMode.value_or(
                base ? base->sensorMode : RunSensorMode::Air);
            add(Field::TargetTemperature);
            add(Field::Preheat);
            add(Field::SensorMode);
            break;
        }
        case FermentationUiManualDraftSlot::ManualTimed: {
            const auto* base =
                manualTimedValues_ ? &*manualTimedValues_ : nullptr;
            view.targetTemperatureCelsius =
                draft.targetTemperatureCelsius.has_value()
                    ? draft.targetTemperatureCelsius
                : base ? std::optional<double>{base->targetTemperatureCelsius}
                       : std::nullopt;
            view.durationMinutes =
                draft.durationMinutes.has_value() ? draft.durationMinutes
                : base ? std::optional<std::uint32_t>{base->durationMinutes}
                       : std::nullopt;
            view.preheat = draft.preheat.value_or(base && base->preheatEnabled);
            view.sensorMode = draft.sensorMode.value_or(
                base ? base->sensorMode : RunSensorMode::Air);
            view.completionMode = draft.completionMode.value_or(
                base ? base->completionMode
                     : CompletionMode::FinishWithoutCooling);
            view.coolingTargetCelsius =
                draft.coolingTargetCelsius.has_value()
                    ? draft.coolingTargetCelsius
                    : (base ? base->coolingTargetCelsius : std::nullopt);
            view.holdDurationMinutes =
                draft.holdDurationMinutes.has_value()
                    ? draft.holdDurationMinutes
                    : (base ? base->holdDurationMinutes : std::nullopt);
            add(Field::TargetTemperature);
            add(Field::Duration);
            add(Field::Preheat);
            add(Field::SensorMode);
            add(Field::CompletionMode);
            if (view.completionMode != CompletionMode::FinishWithoutCooling)
                add(Field::CoolingTarget);
            if (view.completionMode == CompletionMode::CoolAndHoldForDuration)
                add(Field::HoldDuration);
            break;
        }
        case FermentationUiManualDraftSlot::StopCooling:
        case FermentationUiManualDraftSlot::CompletionCooling: {
            const auto& plan =
                slot == FermentationUiManualDraftSlot::StopCooling
                    ? stopCoolingPlan_
                    : completionCoolingPlan_;
            // A cooling plan has one real value: the cooling target.
            view.coolingTargetCelsius =
                draft.targetTemperatureCelsius.has_value()
                    ? draft.targetTemperatureCelsius
                : plan ? std::optional<double>{plan->targetTemperatureCelsius}
                       : std::nullopt;
            add(Field::CoolingTarget);
            view.rowOffset = 1U;
            break;
        }
        case FermentationUiManualDraftSlot::StartCandidate:
            break;
    }
    view.pagerButtons = view.fieldCount > kFermentationUiListVisibleRows;
    return view;
}

// Attaches the manual field list to a page view: pager window, and the
// visible reason while the technical run limits have no released producer
// (fail-closed, O5); a page keeps an own reason if it already has one.
void FermentationTouchWorkspace::applyManualFieldView(
    FermentationUiWorkspaceView& view,
    FermentationUiManualDraftSlot slot) const {
    view.programSummary = makeManualFieldView(slot);
    const auto& list = *view.programSummary;
    view.pager.itemCount = list.fieldCount;
    view.pager.currentIndex =
        list.fieldCount == 0U
            ? 0U
            : std::min(pager_.currentIndex, list.fieldCount - 1U);
    // O5: the technical run limits have no owner yet, so the start of a manual
    // run / cooling plan stays disabled with a visible reason.
    if (!view.blockedReason.has_value()) {
        view.blockedReason = key("manual-parameters-not-released");
    }
}

const ProgramDocument* FermentationTouchWorkspace::storedSelectedProgram(
    const ProgramCatalog* catalog) const {
    if (catalog == nullptr || !selectedProgramId_.has_value()) return nullptr;
    const auto found =
        std::find_if(catalog->programs.begin(), catalog->programs.end(),
                     [this](const ProgramDocument& document) {
                         return document.program.id == *selectedProgramId_;
                     });
    return found == catalog->programs.end() ? nullptr : &*found;
}

ProgramDocument* FermentationTouchWorkspace::ensureProgramCandidate(
    const ProgramCatalog* catalog) {
    if (!programEditCandidate_.has_value()) {
        const auto* stored = storedSelectedProgram(catalog);
        if (stored == nullptr) return nullptr;
        programEditCandidate_ = *stored;
    }
    return &*programEditCandidate_;
}

FermentationUiSettingsView FermentationTouchWorkspace::makeSettingsView(
    const FermentationUiSnapshot& snapshot) const {
    FermentationUiSettingsView settings;
    settings.deviceName = deviceName_;
    // Display convenience only: the Application decides (O4).
    settings.deviceNameEditable = snapshot.home.activeRunId.empty();
    settings.serviceAvailable = snapshot.service.available;
    if (!snapshot.service.available) {
        settings.serviceReason =
            snapshot.service.unavailableReason.value_or(key("service-locked"));
    }
    settings.deviceNameChangeFailed = deviceNameChangeFailed_;
    return settings;
}

FermentationUiProgramEditView FermentationTouchWorkspace::makeProgramEditView(
    const ProgramCatalog* catalog) const {
    FermentationUiProgramEditView view;
    const auto* stored = storedSelectedProgram(catalog);
    if (programEditOperation_ != FermentationUiProgramEditOperation::Edit) {
        // Copy and New list the name only (the request carries it); all other
        // values keep the S6 behaviour.
        view.rows.emplace_back();
        view.rowCount = view.rows.size();
        auto& row = view.rows.back();
        row.field = EditField::Name;
        row.label = key(programFieldLabelKey(EditField::Name));
        if (programEditName_.has_value()) {
            row.text = *programEditName_;
        } else if (programEditOperation_ ==
                       FermentationUiProgramEditOperation::Copy &&
                   stored != nullptr) {
            row.text = stored->program.name + " copy";
        } else {
            row.text = "-";
        }
        row.changed = programEditName_.has_value();
        view.valid = !programEditName_.has_value() ||
                     validateVisibleName(*programEditName_) ==
                         ConfigurationTextStatus::Success;
        return view;
    }
    if (stored == nullptr) return view;
    view.rows.reserve(kFermentationUiProgramFieldCount);
    const auto& document =
        programEditCandidate_.has_value() ? *programEditCandidate_ : *stored;
    const auto makeRow = [](const ProgramDefinition& program, EditField field) {
        FermentationUiProgramEditRow row;
        row.field = field;
        row.label = key(programFieldLabelKey(field));
        switch (field) {
            case EditField::Name:
                row.text = program.name;
                break;
            case EditField::Notes:
                row.text = program.notes.empty() ? "-" : program.notes;
                break;
            case EditField::Preheat:
                row.valueKey = key(program.preheat ? "value-on" : "value-off");
                break;
            case EditField::SensorPreference:
                row.valueKey =
                    sensorPreferenceTextKey(program.sensorPreference);
                break;
            case EditField::FailurePolicy:
                row.valueKey =
                    failurePolicyKey(program.productSensorFailure.policy);
                break;
            case EditField::ReturnStrategy:
                row.valueKey = returnStrategyKey(
                    program.productSensorFailure.returnStrategy);
                break;
            case EditField::CompletionMode:
                row.valueKey = completionModeTextKey(program.completion.mode);
                break;
            default:
                row.text = programNumericDisplay(
                    field, programNumericValue(program, field));
                break;
        }
        return row;
    };
    for (std::size_t index = 0U; index < kFermentationUiProgramFieldCount;
         ++index) {
        const auto field = static_cast<EditField>(index);
        if (!programFieldVisible(document.program, field)) continue;
        auto row = makeRow(document.program, field);
        const auto storedRow = makeRow(stored->program, field);
        row.changed =
            row.text != storedRow.text || row.valueKey != storedRow.valueKey;
        view.rows.push_back(std::move(row));
        view.rowCount = view.rows.size();
    }
    // The candidate must pass the catalog-level program validation and the
    // owning text rules; without a candidate there is nothing to save.
    view.valid =
        programEditCandidate_.has_value() &&
        validateProgram(document, ValidationPurpose::CatalogTemplate).valid() &&
        validateVisibleName(document.program.name) ==
            ConfigurationTextStatus::Success &&
        validateProgramNotes(document.program.notes) ==
            ConfigurationTextStatus::Success;
    return view;
}

void FermentationTouchWorkspace::openTextEdit(FermentationUiTextTarget target,
                                              std::string initial) {
    textTarget_ = target;
    textEdit_.reset(std::move(initial));
    pageStack_.push_back(FermentationUiPage::TextEdit);
    page_ = FermentationUiPage::TextEdit;
    displayLanguageChangeFailed_ = false;
}

FermentationUiWorkspacePress FermentationTouchWorkspace::pressSettingsRow(
    const FermentationUiSnapshot& snapshot, std::size_t row) {
    FermentationUiWorkspacePress result;
    switch (static_cast<FermentationUiSettingsRow>(row)) {
        case FermentationUiSettingsRow::Language:
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateLanguage);
            break;
        case FermentationUiSettingsRow::TimeZone:
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateClock);
            break;
        case FermentationUiSettingsRow::DeviceName:
            // `enabled` guarantees no active run in the snapshot; the
            // Application decides again at the commit.
            deviceNameChangeFailed_ = false;
            openTextEdit(FermentationUiTextTarget::DeviceName, deviceName_);
            result.navigated = true;
            break;
        case FermentationUiSettingsRow::Network:
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateNetwork);
            break;
        case FermentationUiSettingsRow::WebAccess:
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateWebAccess);
            break;
        case FermentationUiSettingsRow::Service:
            result.navigated =
                navigate(FermentationUiWorkspaceSlotAction::NavigateService);
            break;
    }
    static_cast<void>(snapshot);
    return result;
}

FermentationUiWorkspacePress FermentationTouchWorkspace::pressProgramEditCell(
    const FermentationUiWorkspaceView& current,
    const device_platform::DeviceUiTarget& target,
    const ProgramCatalog* catalog) {
    FermentationUiWorkspacePress result;
    const auto& edit = *current.programEdit;
    if (target.column == 1U) {
        result.navigated =
            target.row == 0U ? pager_.moveUp() : pager_.moveDown();
        return result;
    }
    const auto field = edit.rows[current.pager.currentIndex + target.row].field;
    if (programEditOperation_ != FermentationUiProgramEditOperation::Edit) {
        // Copy / New: the name goes into the request.
        openTextEdit(FermentationUiTextTarget::ProgramName,
                     programEditName_.value_or(std::string{}));
        result.navigated = true;
        return result;
    }
    auto* candidate = ensureProgramCandidate(catalog);
    if (candidate == nullptr) return result;
    if (field == EditField::Name || field == EditField::Notes) {
        openTextEdit(field == EditField::Name
                         ? FermentationUiTextTarget::ProgramName
                         : FermentationUiTextTarget::ProgramNotes,
                     field == EditField::Name ? candidate->program.name
                                              : candidate->program.notes);
        result.navigated = true;
        return result;
    }
    if (isNumericProgramField(field)) {
        valueEditProgramField_ = field;
        const auto value = programNumericValue(candidate->program, field);
        valueEdit_.reset(value.has_value() ? programNumericText(field, *value)
                                           : std::string{});
        pageStack_.push_back(FermentationUiPage::ValueEdit);
        page_ = FermentationUiPage::ValueEdit;
        displayLanguageChangeFailed_ = false;
        result.navigated = true;
        return result;
    }
    cycleProgramField(candidate->program, field);
    programEditDirty_ = true;
    result.navigated = true;
    return result;
}

FermentationUiWorkspacePress FermentationTouchWorkspace::pressKeyboardCell(
    const device_platform::DeviceUiTarget& target) {
    FermentationUiWorkspacePress result;
    const auto key = fermentationUiKeyboardKeyAt(textEdit_.mode(), target.row,
                                                 target.column);
    // `enabled` guarantees an existing, allowed key.
    if (key.kind == FermentationUiKeyboardKeyKind::Clear) {
        result.navigated = textEdit_.apply({TextEditAction::Clear, ' '});
    } else if (key.kind == FermentationUiKeyboardKeyKind::Character) {
        result.navigated =
            textEdit_.apply({TextEditAction::Character, key.character});
    }
    return result;
}

FermentationUiWorkspacePress FermentationTouchWorkspace::commitTextEdit(
    const FermentationUiSnapshot& snapshot,
    const FermentationUiWorkspaceView& current) {
    FermentationUiWorkspacePress result;
    // The slot is only enabled for a value the owning text rule accepts.
    if (!current.textEdit.has_value() || !current.textEdit->commitValid ||
        !textEdit_.apply({TextEditAction::Commit, ' '}) ||
        !textEdit_.committedValue().has_value()) {
        return result;
    }
    const auto& value = *textEdit_.committedValue();
    switch (textTarget_) {
        case FermentationUiTextTarget::DeviceName:
            deviceNameChangeFailed_ = false;
            result.setDeviceName = FermentationUiSetDeviceNameCommand{
                value, snapshot.revisions.expectedUserConfigurationRevision};
            break;
        case FermentationUiTextTarget::ProgramName:
            if (programEditOperation_ ==
                FermentationUiProgramEditOperation::Edit) {
                if (programEditCandidate_.has_value())
                    programEditCandidate_->program.name = value;
            } else {
                programEditName_ = value;
            }
            programEditDirty_ = true;
            break;
        case FermentationUiTextTarget::ProgramNotes:
            if (programEditCandidate_.has_value())
                programEditCandidate_->program.notes = value;
            programEditDirty_ = true;
            break;
    }
    result.navigated = goBack();
    return result;
}

void FermentationTouchWorkspace::setPage(FermentationUiPage page) {
    markRenderRelevantChange();
    setCanonicalPageStack(page);
}

}  // namespace fermentation
