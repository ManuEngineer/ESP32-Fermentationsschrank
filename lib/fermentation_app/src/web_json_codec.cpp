#include "web_json_codec.hpp"

#include <cJSON.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "configuration_limits.hpp"
#include "configuration_text.hpp"
#include "program_limits.hpp"
#include "run_snapshot.hpp"

namespace fermentation {
namespace {

struct CJsonDeleter {
    void operator()(cJSON* value) const { cJSON_Delete(value); }
};

using CJsonDocument = std::unique_ptr<cJSON, CJsonDeleter>;

constexpr std::size_t kCJsonPrintSafetyMarginBytes = 5U;
constexpr std::size_t kMaximumRevisionDecimalBytes = 20U;
static_assert(CJSON_NESTING_LIMIT == kMaximumWebJsonNesting,
              "cJSON and the Issue #27 schema depth must stay aligned");

const cJSON* member(const cJSON* object, const char* name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

bool hasOnlyKeys(const cJSON* object,
                 std::initializer_list<const char*> allowed) {
    if (!cJSON_IsObject(object)) return false;
    for (auto* item = object->child; item != nullptr; item = item->next) {
        if (item->string == nullptr) return false;
        bool matched = false;
        for (const auto* key : allowed) {
            if (std::strcmp(item->string, key) == 0) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }
    return true;
}

bool containsForbiddenNulInput(const std::string& body) {
    if (body.find('\0') != std::string::npos) return true;
    constexpr char kDecodedNulEscape[] = {'\\', 'u', '0', '0', '0', '0'};
    if (body.size() < sizeof(kDecodedNulEscape)) return false;
    for (std::size_t index = 0U;
         index <= body.size() - sizeof(kDecodedNulEscape); ++index) {
        if (std::memcmp(body.data() + index, kDecodedNulEscape,
                        sizeof(kDecodedNulEscape)) == 0) {
            return true;
        }
    }
    return false;
}

bool readString(const cJSON* value, std::size_t maximumBytes,
                std::string& output) {
    if (!cJSON_IsString(value) || value->valuestring == nullptr) return false;
    const auto length = std::strlen(value->valuestring);
    if (length == 0U || length > maximumBytes) return false;
    output.assign(value->valuestring, length);
    return true;
}

template <typename Integer>
bool readInteger(const cJSON* value, Integer& output) {
    static_assert(std::is_integral_v<Integer> && std::is_unsigned_v<Integer>,
                  "web DTO integer fields are unsigned");
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) ||
        value->valuedouble < 0.0 ||
        value->valuedouble >
            static_cast<double>(std::numeric_limits<Integer>::max()) ||
        std::floor(value->valuedouble) != value->valuedouble) {
        return false;
    }
    output = static_cast<Integer>(value->valuedouble);
    return true;
}

template <typename Integer>
bool readOptionalInteger(const cJSON* object, const char* key,
                         std::optional<Integer>& output) {
    const auto* value = member(object, key);
    if (value == nullptr) {
        output.reset();
        return true;
    }
    Integer parsed{};
    if (!readInteger(value, parsed)) return false;
    output = parsed;
    return true;
}

bool readRevisionDecimal(const cJSON* value, std::uint64_t& output) {
    if (!cJSON_IsString(value) || value->valuestring == nullptr) return false;
    const auto* text = value->valuestring;
    const auto length = std::strlen(text);
    if (length == 0U || length > kMaximumRevisionDecimalBytes ||
        text[0] == '0') {
        return false;
    }

    std::uint64_t parsed = 0U;
    constexpr auto kMaximum = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t index = 0U; index < length; ++index) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if (byte < static_cast<unsigned char>('0') ||
            byte > static_cast<unsigned char>('9')) {
            return false;
        }
        const auto digit = static_cast<std::uint64_t>(byte - '0');
        if (parsed > (kMaximum - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
    }
    if (parsed == 0U) return false;
    output = parsed;
    return true;
}

bool readOptionalRevision(const cJSON* object, const char* key,
                          std::optional<std::uint64_t>& output) {
    const auto* value = member(object, key);
    if (value == nullptr) {
        output.reset();
        return true;
    }
    std::uint64_t parsed{};
    if (!readRevisionDecimal(value, parsed)) return false;
    output = parsed;
    return true;
}

bool readDouble(const cJSON* value, double& output) {
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble)) {
        return false;
    }
    output = value->valuedouble;
    return true;
}

bool readOptionalDouble(const cJSON* object, const char* key,
                        std::optional<double>& output) {
    const auto* value = member(object, key);
    if (value == nullptr) {
        output.reset();
        return true;
    }
    double parsed{};
    if (!readDouble(value, parsed)) return false;
    output = parsed;
    return true;
}

bool readOptionalBool(const cJSON* object, const char* key,
                      std::optional<bool>& output) {
    const auto* value = member(object, key);
    if (value == nullptr) {
        output.reset();
        return true;
    }
    if (!cJSON_IsBool(value)) return false;
    output = cJSON_IsTrue(value);
    return true;
}

bool readRequiredBool(const cJSON* object, const char* key, bool& output) {
    const auto* value = member(object, key);
    if (!cJSON_IsBool(value)) return false;
    output = cJSON_IsTrue(value);
    return true;
}

template <typename Enum, std::size_t Count>
bool readEnum(const cJSON* value,
              const std::array<std::pair<const char*, Enum>, Count>& choices,
              Enum& output) {
    std::string text;
    if (!readString(value, 40U, text)) return false;
    for (const auto& choice : choices) {
        if (text == choice.first) {
            output = choice.second;
            return true;
        }
    }
    return false;
}

constexpr std::array<std::pair<const char*, RunSensorMode>, 2U> kSensorModes{
    {{"product", RunSensorMode::Product}, {"air", RunSensorMode::Air}}};
constexpr std::array<std::pair<const char*, CompletionMode>, 4U>
    kCompletionModes{
        {{"finish-without-cooling", CompletionMode::FinishWithoutCooling},
         {"cool-then-finish", CompletionMode::CoolThenFinish},
         {"cool-and-hold-for-duration", CompletionMode::CoolAndHoldForDuration},
         {"cool-and-hold-until-manual-stop",
          CompletionMode::CoolAndHoldUntilManualStop}}};
constexpr std::array<std::pair<const char*, StopOption>, 3U> kStopOptions{{
    {"back", StopOption::Back},
    {"abort-and-turn-off", StopOption::AbortAndTurnOff},
    {"abort-and-cool", StopOption::AbortAndCool},
}};
constexpr std::array<std::pair<const char*, SensorSelectionUserAction>, 3U>
    kSensorActions{
        {{"continue-with-air", SensorSelectionUserAction::ContinueWithAir},
         {"return-to-product", SensorSelectionUserAction::ReturnToProduct},
         {"recheck-product", SensorSelectionUserAction::RecheckProduct}}};

bool readOptionalEnum(const cJSON* object, const char* key,
                      std::optional<RunSensorMode>& output) {
    const auto* value = member(object, key);
    if (value == nullptr) {
        output.reset();
        return true;
    }
    RunSensorMode parsed{};
    if (!readEnum(value, kSensorModes, parsed)) return false;
    output = parsed;
    return true;
}

bool readOptionalCompletionMode(const cJSON* object, const char* key,
                                std::optional<CompletionMode>& output) {
    const auto* value = member(object, key);
    if (value == nullptr) {
        output.reset();
        return true;
    }
    CompletionMode parsed{};
    if (!readEnum(value, kCompletionModes, parsed)) return false;
    output = parsed;
    return true;
}

bool validTemperature(double value) {
    return std::isfinite(value) &&
           value >= program_limits::kMinimumFermentationTemperatureCelsius &&
           value <= program_limits::kMaximumFermentationTemperatureCelsius;
}

bool validQualificationBand(double value) {
    return std::isfinite(value) &&
           value >= program_limits::kMinimumQualificationBandCelsius &&
           value <= program_limits::kMaximumQualificationBandCelsius;
}

bool validCoolingTarget(double value) {
    return std::isfinite(value) &&
           value >= program_limits::kMinimumCoolingTargetCelsius &&
           value <= program_limits::kMaximumCoolingTargetCelsius;
}

bool validManualPlanValues(const FermentationUiManualRunPlanValues& values) {
    const bool waitMatchesPreheat =
        values.preheatEnabled == values.maximumProductWaitMinutes.has_value();
    const bool waitValid = !values.maximumProductWaitMinutes.has_value() ||
                           (*values.maximumProductWaitMinutes >=
                                program_limits::kMinimumProductWaitMinutes &&
                            *values.maximumProductWaitMinutes <=
                                program_limits::kMaximumProductWaitMinutes);
    return validTemperature(values.targetTemperatureCelsius) &&
           validQualificationBand(values.qualificationBandCelsius) &&
           values.qualificationDurationMinutes >=
               program_limits::kMinimumQualificationDurationMinutes &&
           values.qualificationDurationMinutes <=
               program_limits::kMaximumQualificationDurationMinutes &&
           values.maximumTargetReachMinutes >=
               program_limits::kMinimumTargetReachMinutes &&
           values.maximumTargetReachMinutes <=
               program_limits::kMaximumTargetReachMinutes &&
           waitMatchesPreheat && waitValid;
}

bool validCompletionCombination(
    CompletionMode mode, const std::optional<double>& coolingTarget,
    const std::optional<std::uint32_t>& holdDuration) {
    const bool coolingTargetValid =
        !coolingTarget.has_value() || validCoolingTarget(*coolingTarget);
    const bool holdDurationValid =
        !holdDuration.has_value() ||
        *holdDuration >= program_limits::kMinimumHoldDurationMinutes;
    if (!coolingTargetValid || !holdDurationValid) return false;

    switch (mode) {
        case CompletionMode::FinishWithoutCooling:
            return !coolingTarget.has_value() && !holdDuration.has_value();
        case CompletionMode::CoolThenFinish:
        case CompletionMode::CoolAndHoldUntilManualStop:
            return coolingTarget.has_value() && !holdDuration.has_value();
        case CompletionMode::CoolAndHoldForDuration:
            return coolingTarget.has_value() && holdDuration.has_value();
    }
    return false;
}

bool validOptionalCompletionCombination(
    const std::optional<CompletionMode>& mode,
    const std::optional<double>& coolingTarget,
    const std::optional<std::uint32_t>& holdDuration) {
    const bool coolingTargetValid =
        !coolingTarget.has_value() || validCoolingTarget(*coolingTarget);
    const bool holdDurationValid =
        !holdDuration.has_value() ||
        *holdDuration >= program_limits::kMinimumHoldDurationMinutes;
    return coolingTargetValid && holdDurationValid &&
           (!mode.has_value() ||
            validCompletionCombination(*mode, coolingTarget, holdDuration));
}

bool validStartCandidate(const FermentationUiStartCandidate& candidate) {
    const bool targetValid =
        !candidate.targetTemperatureCelsius.has_value() ||
        validTemperature(*candidate.targetTemperatureCelsius);
    const bool durationValid =
        !candidate.fermentationDurationMinutes.has_value() ||
        (*candidate.fermentationDurationMinutes >=
             program_limits::kMinimumFermentationDurationMinutes &&
         *candidate.fermentationDurationMinutes <=
             program_limits::kMaximumFermentationDurationMinutes);
    return targetValid && durationValid &&
           validOptionalCompletionCombination(candidate.completionMode,
                                              candidate.coolingTargetCelsius,
                                              candidate.holdDurationMinutes);
}

bool validManualTimedValues(const ManualTimedRunValues& values) {
    ManualTimedRunSource source;
    source.stage.targetTemperatureCelsius = values.targetTemperatureCelsius;
    source.stage.durationMinutes = values.durationMinutes;
    source.preheatEnabled = values.preheatEnabled;
    source.maximumProductWaitMinutes = values.maximumProductWaitMinutes;
    source.targetQualification.bandCelsius = values.qualificationBandCelsius;
    source.targetQualification.durationMinutes =
        values.qualificationDurationMinutes;
    source.maximumTargetReachMinutes = values.maximumTargetReachMinutes;
    source.completion.mode = values.completionMode;
    source.completion.coolingTargetCelsius = values.coolingTargetCelsius;
    source.completion.holdDurationMinutes = values.holdDurationMinutes;
    return validateManualTimedRunSource(source);
}

bool readManualPlan(const cJSON* value,
                    FermentationUiManualRunPlanValues& output) {
    if (!cJSON_IsObject(value) ||
        !hasOnlyKeys(value, {"x", "s", "h", "w", "q", "qd", "tr"})) {
        return false;
    }
    if (!readDouble(member(value, "x"), output.targetTemperatureCelsius) ||
        !readEnum(member(value, "s"), kSensorModes, output.sensorMode) ||
        !readRequiredBool(value, "h", output.preheatEnabled) ||
        !readOptionalInteger(value, "w", output.maximumProductWaitMinutes) ||
        !readDouble(member(value, "q"), output.qualificationBandCelsius) ||
        !readInteger(member(value, "qd"),
                     output.qualificationDurationMinutes) ||
        !readInteger(member(value, "tr"), output.maximumTargetReachMinutes)) {
        return false;
    }
    return validManualPlanValues(output);
}

bool readExpected(const cJSON* value, FermentationUiExpectedRevisions& output) {
    if (!cJSON_IsObject(value) ||
        !hasOnlyKeys(value, {"s", "r", "m", "f", "e", "u", "c"}) ||
        !readInteger(member(value, "s"), output.expectedStateSequence) ||
        !readOptionalInteger(value, "r", output.expectedRunRevision) ||
        !readOptionalInteger(value, "m", output.expectedMessageRevision) ||
        !readOptionalInteger(value, "f", output.expectedFaultRevision) ||
        !readOptionalInteger(value, "e",
                             output.expectedRecoveryEpisodeRevision)) {
        return false;
    }
    std::optional<std::uint64_t> userRevision;
    std::optional<std::uint64_t> catalogRevision;
    if (!readOptionalRevision(value, "u", userRevision) ||
        !readOptionalRevision(value, "c", catalogRevision)) {
        return false;
    }
    if (userRevision.has_value()) {
        output.expectedUserConfigurationRevision =
            UserConfigurationRevision{*userRevision};
    }
    if (catalogRevision.has_value()) {
        output.expectedProgramCatalogRevision =
            ProgramCatalogRevision{*catalogRevision};
    }
    return true;
}

bool readIntent(const cJSON* value, FermentationUiEnvelopePayload& output) {
    if (!cJSON_IsObject(value)) return false;
    std::string type;
    if (!readString(member(value, "t"), 40U, type)) return false;

    if (type == "start-program") {
        const auto* candidateValue = member(value, "c");
        if (!hasOnlyKeys(value, {"t", "c"}) ||
            !cJSON_IsObject(candidateValue) ||
            !hasOnlyKeys(candidateValue,
                         {"p", "x", "d", "h", "s", "c", "k", "l"})) {
            return false;
        }
        FermentationUiStartCandidate candidate;
        if (!readString(member(candidateValue, "p"),
                        configuration_limits::kMaximumProgramIdBytes,
                        candidate.programId) ||
            validateLowercaseIdentifier(
                candidate.programId,
                configuration_limits::kMinimumProgramIdBytes,
                configuration_limits::kMaximumProgramIdBytes) !=
                ConfigurationTextStatus::Success ||
            !readOptionalDouble(candidateValue, "x",
                                candidate.targetTemperatureCelsius) ||
            !readOptionalInteger(candidateValue, "d",
                                 candidate.fermentationDurationMinutes) ||
            !readOptionalBool(candidateValue, "h", candidate.preheatEnabled) ||
            !readOptionalEnum(candidateValue, "s", candidate.sensorMode) ||
            !readOptionalCompletionMode(candidateValue, "c",
                                        candidate.completionMode) ||
            !readOptionalDouble(candidateValue, "k",
                                candidate.coolingTargetCelsius) ||
            !readOptionalInteger(candidateValue, "l",
                                 candidate.holdDurationMinutes)) {
            return false;
        }
        if (!validStartCandidate(candidate)) return false;
        output = FermentationUiStartProgramIntent{std::move(candidate)};
        return true;
    }

    if (type == "start-manual-timed") {
        if (!hasOnlyKeys(value, {"t", "x", "d", "s", "h", "w", "q", "qd", "tr",
                                 "c", "k", "l"})) {
            return false;
        }
        ManualTimedRunValues values;
        if (!readDouble(member(value, "x"), values.targetTemperatureCelsius) ||
            !readInteger(member(value, "d"), values.durationMinutes) ||
            !readEnum(member(value, "s"), kSensorModes, values.sensorMode) ||
            !readRequiredBool(value, "h", values.preheatEnabled) ||
            !readOptionalInteger(value, "w",
                                 values.maximumProductWaitMinutes) ||
            !readDouble(member(value, "q"), values.qualificationBandCelsius) ||
            !readInteger(member(value, "qd"),
                         values.qualificationDurationMinutes) ||
            !readInteger(member(value, "tr"),
                         values.maximumTargetReachMinutes) ||
            !readEnum(member(value, "c"), kCompletionModes,
                      values.completionMode) ||
            !readOptionalDouble(value, "k", values.coolingTargetCelsius) ||
            !readOptionalInteger(value, "l", values.holdDurationMinutes)) {
            return false;
        }
        if (!validManualTimedValues(values)) return false;
        output = FermentationUiStartManualTimedIntent{values};
        return true;
    }

    if (type == "start-manual-holding") {
        if (!hasOnlyKeys(value, {"t", "p"})) return false;
        FermentationUiManualRunPlanValues plan;
        if (!readManualPlan(member(value, "p"), plan)) return false;
        output = FermentationUiStartManualHoldingIntent{std::move(plan)};
        return true;
    }

    if (type == "stop-run") {
        if (!hasOnlyKeys(value, {"t", "o", "p"})) return false;
        StopOption option{};
        if (!readEnum(member(value, "o"), kStopOptions, option)) return false;
        std::optional<FermentationUiManualRunPlanValues> plan;
        if (const auto* planValue = member(value, "p"); planValue != nullptr) {
            FermentationUiManualRunPlanValues parsed;
            if (!readManualPlan(planValue, parsed)) return false;
            plan = std::move(parsed);
        }
        if ((option == StopOption::AbortAndCool) != plan.has_value()) {
            return false;
        }
        output = FermentationUiStopRunIntent{option, std::move(plan)};
        return true;
    }

    if (type == "complete-run") {
        if (!hasOnlyKeys(value, {"t", "c", "p"})) return false;
        bool startCooling{};
        if (!readRequiredBool(value, "c", startCooling)) return false;
        std::optional<FermentationUiManualRunPlanValues> plan;
        if (const auto* planValue = member(value, "p"); planValue != nullptr) {
            FermentationUiManualRunPlanValues parsed;
            if (!readManualPlan(planValue, parsed)) return false;
            plan = std::move(parsed);
        }
        if (startCooling != plan.has_value()) return false;
        output = FermentationUiCompleteRunIntent{startCooling, std::move(plan)};
        return true;
    }

    if (type == "adjust-run") {
        if (!hasOnlyKeys(value, {"t", "x", "d"})) return false;
        FermentationUiAdjustRunIntent intent;
        if (!readOptionalDouble(value, "x", intent.targetTemperatureCelsius) ||
            !readOptionalInteger(value, "d", intent.remainingDurationMinutes)) {
            return false;
        }
        if (intent.targetTemperatureCelsius.has_value() &&
            !validTemperature(*intent.targetTemperatureCelsius)) {
            return false;
        }
        if (intent.remainingDurationMinutes.has_value() &&
            *intent.remainingDurationMinutes != 0U &&
            (*intent.remainingDurationMinutes <
                 program_limits::kMinimumFermentationDurationMinutes ||
             *intent.remainingDurationMinutes >
                 program_limits::kMaximumFermentationDurationMinutes)) {
            return false;
        }
        output = intent;
        return true;
    }

    if (type == "recovery-time-correction") {
        if (!hasOnlyKeys(value, {"t", "d"})) return false;
        FermentationUiRecoveryTimeCorrectionIntent intent;
        if (!readInteger(member(value, "d"), intent.secondsDelta)) return false;
        output = intent;
        return true;
    }

    if (type == "ack-message" || type == "mute-message") {
        if (!hasOnlyKeys(value, {"t", "id"})) return false;
        std::uint32_t id{};
        if (!readInteger(member(value, "id"), id)) return false;
        output =
            type == "ack-message"
                ? FermentationUiEnvelopePayload{FermentationUiAcknowledgeMessageIntent{
                      id}}
                : FermentationUiEnvelopePayload{
                      FermentationUiMuteMessageIntent{id}};
        return true;
    }

    if (type == "reset-fault") {
        if (!hasOnlyKeys(value, {"t"})) return false;
        output = FermentationUiResetFaultIntent{};
        return true;
    }

    if (type == "sensor-selection") {
        if (!hasOnlyKeys(value, {"t", "a"})) return false;
        SensorSelectionUserAction action{};
        if (!readEnum(member(value, "a"), kSensorActions, action)) return false;
        output = FermentationUiSensorSelectionIntent{action};
        return true;
    }

    return false;
}

bool addString(cJSON* object, const char* key, const char* value) {
    return value != nullptr &&
           cJSON_AddStringToObject(object, key, value) != nullptr;
}

bool addNumber(cJSON* object, const char* key, double value) {
    return std::isfinite(value) &&
           cJSON_AddNumberToObject(object, key, value) != nullptr;
}

bool serializeBounded(cJSON* document, std::string& output) {
    constexpr auto kBufferBytes =
        kMaximumWebApiResponseBodyBytes + kCJsonPrintSafetyMarginBytes + 1U;
    static_assert(kBufferBytes <=
                      static_cast<std::size_t>(std::numeric_limits<int>::max()),
                  "cJSON print buffer length must fit its API");
    std::array<char, kBufferBytes> buffer{};
    if (document == nullptr ||
        !cJSON_PrintPreallocated(document, buffer.data(),
                                 static_cast<int>(buffer.size()), false)) {
        return false;
    }
    const auto* terminator = static_cast<const char*>(
        std::memchr(buffer.data(), '\0', buffer.size()));
    if (terminator == nullptr) return false;
    const auto length = static_cast<std::size_t>(terminator - buffer.data());
    if (length == 0U || length > kMaximumWebApiResponseBodyBytes) return false;
    output.assign(buffer.data(), length);
    return true;
}

const char* homeModeName(FermentationHomeMode value) {
    switch (value) {
        case FermentationHomeMode::Standby:
            return "standby";
        case FermentationHomeMode::ActiveRun:
            return "active-run";
        case FermentationHomeMode::Waiting:
            return "waiting";
        case FermentationHomeMode::Completed:
            return "completed";
        case FermentationHomeMode::Restricted:
            return "restricted";
        case FermentationHomeMode::Recovery:
            return "recovery";
        case FermentationHomeMode::Unavailable:
            return "unavailable";
    }
    return nullptr;
}

const char* processStateName(ProcessState value) {
    switch (value) {
        case ProcessState::Boot:
            return "boot";
        case ProcessState::SafeBoot:
            return "safe-boot";
        case ProcessState::Standby:
            return "standby";
        case ProcessState::Preheating:
            return "preheating";
        case ProcessState::WaitingForProduct:
            return "waiting-for-product";
        case ProcessState::ReachingTarget:
            return "reaching-target";
        case ProcessState::QualifyingTarget:
            return "qualifying-target";
        case ProcessState::Fermenting:
            return "fermenting";
        case ProcessState::Cooling:
            return "cooling";
        case ProcessState::CoolHolding:
            return "cool-holding";
        case ProcessState::ManualHolding:
            return "manual-holding";
        case ProcessState::Completed:
            return "completed";
        case ProcessState::RecoveryEvaluation:
            return "recovery-evaluation";
        case ProcessState::Fault:
            return "fault";
        case ProcessState::ServiceMode:
            return "service-mode";
    }
    return nullptr;
}

const char* networkModeName(device_platform::NetworkMode value,
                            bool selectionRequired) {
    if (selectionRequired) return "selection-required";
    switch (value) {
        case device_platform::NetworkMode::AP_ONLY:
            return "ap-only";
        case device_platform::NetworkMode::HOME_WIFI:
            return "home-wifi";
        case device_platform::NetworkMode::UNSELECTED:
            return "selection-required";
    }
    return nullptr;
}

const char* temperatureRoleName(FermentationTemperatureRole value) {
    switch (value) {
        case FermentationTemperatureRole::CabinetAir:
            return "cabinet-air";
        case FermentationTemperatureRole::Product:
            return "product";
        case FermentationTemperatureRole::Cooling:
            return "cooling";
    }
    return nullptr;
}

const char* qualityName(device_platform::SensorQuality value) {
    switch (value) {
        case device_platform::SensorQuality::Valid:
            return "valid";
        case device_platform::SensorQuality::Stale:
            return "stale";
        case device_platform::SensorQuality::Failed:
            return "failed";
    }
    return nullptr;
}

const char* messageCodeName(MessageCode value) {
    switch (value) {
        case MessageCode::ProductInsertionRequested:
            return "product-insertion-requested";
        case MessageCode::TargetReachTimeExceeded:
            return "target-reach-time-exceeded";
        case MessageCode::UserDecisionRequired:
            return "user-decision-required";
        case MessageCode::RunCompleted:
            return "run-completed";
        case MessageCode::RunAborted:
            return "run-aborted";
        case MessageCode::RecoveryPending:
            return "recovery-pending";
        case MessageCode::SafetyFault:
            return "safety-fault";
    }
    return nullptr;
}

const char* messageSeverityName(MessageClass value) {
    switch (value) {
        case MessageClass::Information:
            return "information";
        case MessageClass::ProcessWarning:
            return "warning";
        case MessageClass::Recovery:
            return "recovery";
        case MessageClass::DecisionRequired:
            return "decision-required";
        case MessageClass::SafetyFault:
            return "safety-fault";
    }
    return nullptr;
}

}  // namespace

WebRunMutationDecodeStatus decodeWebRunMutation(const std::string& exactBody,
                                                WebRunMutationDto& output) {
    if (exactBody.empty()) return WebRunMutationDecodeStatus::Invalid;
    if (exactBody.size() > kMaximumWebRunMutationBodyBytes) {
        return WebRunMutationDecodeStatus::TooLarge;
    }
    if (containsForbiddenNulInput(exactBody)) {
        return WebRunMutationDecodeStatus::Invalid;
    }

    CJsonDocument document(cJSON_ParseWithLengthOpts(
        exactBody.c_str(), exactBody.size() + 1U, nullptr, true));
    if (!document || !cJSON_IsObject(document.get())) {
        return WebRunMutationDecodeStatus::Invalid;
    }
    const auto* const root = document.get();
    std::uint32_t version{};
    if (!hasOnlyKeys(root, {"v", "r", "i"}) ||
        !readInteger(member(root, "v"), version) || version != 1U) {
        return WebRunMutationDecodeStatus::Invalid;
    }

    WebRunMutationDto parsed;
    if (!readExpected(member(root, "r"), parsed.expected) ||
        !readIntent(member(root, "i"), parsed.intent)) {
        return WebRunMutationDecodeStatus::Invalid;
    }
    output = std::move(parsed);
    return WebRunMutationDecodeStatus::Success;
}

bool encodeWebApiStatus(const FermentationUiSnapshot& snapshot,
                        std::string& output) {
    const auto* homeMode = homeModeName(snapshot.home.mode);
    const auto* processState = processStateName(snapshot.home.processState);
    const auto* networkMode = networkModeName(
        snapshot.network.currentMode, snapshot.network.selectionRequired);
    if (homeMode == nullptr || processState == nullptr ||
        networkMode == nullptr) {
        return false;
    }

    CJsonDocument document(cJSON_CreateObject());
    if (!document ||
        cJSON_AddNumberToObject(document.get(), "version", 1.0) == nullptr ||
        cJSON_AddBoolToObject(document.get(), "ready", snapshot.status.ready) ==
            nullptr ||
        !addString(document.get(), "homeMode", homeMode) ||
        !addString(document.get(), "processState", processState) ||
        !addString(document.get(), "networkMode", networkMode) ||
        cJSON_AddBoolToObject(document.get(), "selectionRequired",
                              snapshot.network.selectionRequired) == nullptr) {
        return false;
    }
    auto* revisions = cJSON_AddObjectToObject(document.get(), "revisions");
    if (revisions == nullptr ||
        cJSON_AddNumberToObject(revisions, "stateSequence",
                                snapshot.revisions.expectedStateSequence) ==
            nullptr) {
        return false;
    }
    if (snapshot.revisions.expectedRunRevision.has_value() &&
        cJSON_AddNumberToObject(revisions, "run",
                                *snapshot.revisions.expectedRunRevision) ==
            nullptr) {
        return false;
    }
    if (snapshot.revisions.expectedMessageRevision.has_value() &&
        cJSON_AddNumberToObject(revisions, "messages",
                                *snapshot.revisions.expectedMessageRevision) ==
            nullptr) {
        return false;
    }
    if (snapshot.revisions.expectedFaultRevision.has_value() &&
        cJSON_AddNumberToObject(revisions, "fault",
                                *snapshot.revisions.expectedFaultRevision) ==
            nullptr) {
        return false;
    }
    if (snapshot.revisions.expectedRecoveryEpisodeRevision.has_value() &&
        cJSON_AddNumberToObject(
            revisions, "recoveryEpisode",
            *snapshot.revisions.expectedRecoveryEpisodeRevision) == nullptr) {
        return false;
    }
    if (snapshot.revisions.expectedUserConfigurationRevision.has_value()) {
        const auto revision = std::to_string(
            snapshot.revisions.expectedUserConfigurationRevision->value());
        if (!addString(revisions, "userConfiguration", revision.c_str())) {
            return false;
        }
    }
    if (snapshot.revisions.expectedProgramCatalogRevision.has_value()) {
        const auto revision = std::to_string(
            snapshot.revisions.expectedProgramCatalogRevision->value());
        if (!addString(revisions, "programCatalog", revision.c_str())) {
            return false;
        }
    }
    return serializeBounded(document.get(), output);
}

bool encodeWebApiTemperatures(const FermentationUiSnapshot& snapshot,
                              std::string& output) {
    if (snapshot.temperatures.size() > 3U) return false;
    CJsonDocument document(cJSON_CreateObject());
    if (!document ||
        cJSON_AddNumberToObject(document.get(), "version", 1.0) == nullptr) {
        return false;
    }
    auto* temperatures = cJSON_AddArrayToObject(document.get(), "temperatures");
    if (temperatures == nullptr) return false;
    for (const auto& view : snapshot.temperatures) {
        const auto* role = temperatureRoleName(view.role);
        const auto* quality = qualityName(view.quality.quality);
        if (role == nullptr || quality == nullptr) return false;
        const bool valid =
            view.quality.quality == device_platform::SensorQuality::Valid &&
            view.valueCelsius.has_value() && std::isfinite(*view.valueCelsius);
        auto* item = cJSON_CreateObject();
        if (item == nullptr) return false;
        const bool added =
            addString(item, "role", role) &&
            addString(item, "quality", quality) &&
            cJSON_AddBoolToObject(item, "valid", valid) != nullptr &&
            (valid ? addNumber(item, "valueCelsius", *view.valueCelsius)
                   : cJSON_AddNullToObject(item, "valueCelsius") != nullptr);
        if (!added || !cJSON_AddItemToArray(temperatures, item)) {
            cJSON_Delete(item);
            return false;
        }
    }
    return serializeBounded(document.get(), output);
}

bool encodeWebApiAlerts(const FermentationUiSnapshot& snapshot,
                        std::string& output) {
    if (snapshot.messages.size() > kMaximumWebApiAlertCount) return false;
    CJsonDocument document(cJSON_CreateObject());
    if (!document ||
        cJSON_AddNumberToObject(document.get(), "version", 1.0) == nullptr) {
        return false;
    }
    auto* alerts = cJSON_AddArrayToObject(document.get(), "alerts");
    if (alerts == nullptr) return false;
    for (const auto& view : snapshot.messages) {
        const auto& message = view.message;
        const auto* code = messageCodeName(message.code);
        const auto* severity = messageSeverityName(message.messageClass);
        if (code == nullptr || severity == nullptr) return false;
        auto* item = cJSON_CreateObject();
        if (item == nullptr) return false;
        const bool added =
            cJSON_AddNumberToObject(item, "id", message.id) != nullptr &&
            addString(item, "code", code) &&
            addString(item, "severity", severity) &&
            cJSON_AddBoolToObject(item, "active", message.active) != nullptr &&
            cJSON_AddBoolToObject(item, "acknowledged", message.acknowledged) !=
                nullptr &&
            cJSON_AddBoolToObject(item, "resolved", message.resolved) !=
                nullptr &&
            cJSON_AddBoolToObject(item, "decisionRequired",
                                  message.decisionRequired) != nullptr &&
            cJSON_AddBoolToObject(item, "muted", message.acousticMuted) !=
                nullptr &&
            cJSON_AddNumberToObject(item, "revision", message.revision) !=
                nullptr;
        if (!added || !cJSON_AddItemToArray(alerts, item)) {
            cJSON_Delete(item);
            return false;
        }
    }
    return serializeBounded(document.get(), output);
}

}  // namespace fermentation
