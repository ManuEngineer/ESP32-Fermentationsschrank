#include "web_json_codec.hpp"

#include <ArduinoJson.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "configuration_limits.hpp"

namespace fermentation {
namespace {

using JsonObjectConst = ArduinoJson::JsonObjectConst;
using JsonVariantConst = ArduinoJson::JsonVariantConst;

bool hasOnlyKeys(JsonObjectConst object,
                 std::initializer_list<const char*> allowed) {
    for (const auto pair : object) {
        const auto memberName = pair.key();
        const auto memberLength = memberName.size();
        if (memberName.isNull() || memberName.c_str() == nullptr ||
            std::memchr(memberName.c_str(), '\0', memberLength) != nullptr) {
            return false;
        }
        bool matched = false;
        for (const auto* key : allowed) {
            const auto keyLength = std::strlen(key);
            if (memberLength == keyLength &&
                std::memcmp(memberName.c_str(), key, keyLength) == 0) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }
    return true;
}

bool readString(JsonVariantConst value, std::size_t maximumBytes,
                std::string& output) {
    if (!value.is<ArduinoJson::JsonString>()) return false;
    const auto text = value.as<ArduinoJson::JsonString>();
    const auto length = text.size();
    if (text.isNull() || text.c_str() == nullptr ||
        std::memchr(text.c_str(), '\0', length) != nullptr) {
        return false;
    }
    if (length == 0U || length > maximumBytes) return false;
    output.assign(text.c_str(), length);
    return true;
}

template <typename Integer>
bool readInteger(JsonVariantConst value, Integer& output) {
    static_assert(std::is_integral_v<Integer> && std::is_unsigned_v<Integer>,
                  "web DTO integer fields are unsigned");
    if (!value.is<Integer>()) return false;
    output = value.as<Integer>();
    return true;
}

template <typename Integer>
bool readOptionalInteger(JsonObjectConst object, const char* key,
                         std::optional<Integer>& output) {
    if (object[key].isUnbound()) {
        output.reset();
        return true;
    }
    Integer value{};
    if (!readInteger(object[key], value)) return false;
    output = value;
    return true;
}

bool readDouble(JsonVariantConst value, double& output) {
    if (!value.is<double>()) return false;
    const auto number = value.as<double>();
    if (!std::isfinite(number)) return false;
    output = number;
    return true;
}

bool readOptionalDouble(JsonObjectConst object, const char* key,
                        std::optional<double>& output) {
    if (object[key].isUnbound()) {
        output.reset();
        return true;
    }
    double value{};
    if (!readDouble(object[key], value)) return false;
    output = value;
    return true;
}

bool readOptionalBool(JsonObjectConst object, const char* key,
                      std::optional<bool>& output) {
    if (object[key].isUnbound()) {
        output.reset();
        return true;
    }
    const auto value = object[key];
    if (!value.is<bool>()) return false;
    output = value.as<bool>();
    return true;
}

bool readRequiredBool(JsonObjectConst object, const char* key, bool& output) {
    const auto value = object[key];
    if (!value.is<bool>()) return false;
    output = value.as<bool>();
    return true;
}

template <typename Enum, std::size_t Count>
bool readEnum(JsonVariantConst value,
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

bool readOptionalEnum(JsonObjectConst object, const char* key,
                      std::optional<RunSensorMode>& output) {
    if (object[key].isUnbound()) {
        output.reset();
        return true;
    }
    RunSensorMode value{};
    if (!readEnum(object[key], kSensorModes, value)) return false;
    output = value;
    return true;
}

bool readOptionalCompletionMode(JsonObjectConst object, const char* key,
                                std::optional<CompletionMode>& output) {
    if (object[key].isUnbound()) {
        output.reset();
        return true;
    }
    CompletionMode value{};
    if (!readEnum(object[key], kCompletionModes, value)) return false;
    output = value;
    return true;
}

bool readManualPlan(JsonVariantConst value,
                    FermentationUiManualRunPlanValues& output) {
    if (!value.is<JsonObjectConst>()) return false;
    const auto object = value.as<JsonObjectConst>();
    if (!hasOnlyKeys(object, {"x", "s", "h", "w", "q", "qd", "tr"})) {
        return false;
    }
    if (!readDouble(object["x"], output.targetTemperatureCelsius) ||
        !readEnum(object["s"], kSensorModes, output.sensorMode) ||
        !readRequiredBool(object, "h", output.preheatEnabled) ||
        !readOptionalInteger(object, "w", output.maximumProductWaitMinutes) ||
        !readDouble(object["q"], output.qualificationBandCelsius) ||
        !readInteger(object["qd"], output.qualificationDurationMinutes) ||
        !readInteger(object["tr"], output.maximumTargetReachMinutes)) {
        return false;
    }
    return true;
}

bool readExpected(JsonVariantConst value,
                  FermentationUiExpectedRevisions& output) {
    if (!value.is<JsonObjectConst>()) return false;
    const auto object = value.as<JsonObjectConst>();
    if (!hasOnlyKeys(object, {"s", "r", "m", "f", "e", "u", "c"}) ||
        !readInteger(object["s"], output.expectedStateSequence) ||
        !readOptionalInteger(object, "r", output.expectedRunRevision) ||
        !readOptionalInteger(object, "m", output.expectedMessageRevision) ||
        !readOptionalInteger(object, "f", output.expectedFaultRevision) ||
        !readOptionalInteger(object, "e",
                             output.expectedRecoveryEpisodeRevision)) {
        return false;
    }
    std::optional<std::uint64_t> userRevision;
    std::optional<std::uint64_t> catalogRevision;
    if (!readOptionalInteger(object, "u", userRevision) ||
        !readOptionalInteger(object, "c", catalogRevision)) {
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

bool readIntent(JsonVariantConst value, FermentationUiEnvelopePayload& output) {
    if (!value.is<JsonObjectConst>()) return false;
    const auto object = value.as<JsonObjectConst>();
    std::string type;
    if (!readString(object["t"], 40U, type)) return false;

    if (type == "start-program") {
        if (!hasOnlyKeys(object, {"t", "c"}) ||
            !object["c"].is<JsonObjectConst>()) {
            return false;
        }
        const auto candidateObject = object["c"].as<JsonObjectConst>();
        if (!hasOnlyKeys(candidateObject,
                         {"p", "x", "d", "h", "s", "c", "k", "l"})) {
            return false;
        }
        FermentationUiStartCandidate candidate;
        if (!readString(candidateObject["p"],
                        configuration_limits::kMaximumProgramIdBytes,
                        candidate.programId) ||
            !readOptionalDouble(candidateObject, "x",
                                candidate.targetTemperatureCelsius) ||
            !readOptionalInteger(candidateObject, "d",
                                 candidate.fermentationDurationMinutes) ||
            !readOptionalBool(candidateObject, "h", candidate.preheatEnabled) ||
            !readOptionalEnum(candidateObject, "s", candidate.sensorMode) ||
            !readOptionalCompletionMode(candidateObject, "c",
                                        candidate.completionMode) ||
            !readOptionalDouble(candidateObject, "k",
                                candidate.coolingTargetCelsius) ||
            !readOptionalInteger(candidateObject, "l",
                                 candidate.holdDurationMinutes)) {
            return false;
        }
        output = FermentationUiStartProgramIntent{std::move(candidate)};
        return true;
    }

    if (type == "start-manual-timed") {
        if (!hasOnlyKeys(object, {"t", "x", "d", "s", "h", "w", "q", "qd", "tr",
                                  "c", "k", "l"})) {
            return false;
        }
        ManualTimedRunValues values;
        if (!readDouble(object["x"], values.targetTemperatureCelsius) ||
            !readInteger(object["d"], values.durationMinutes) ||
            !readEnum(object["s"], kSensorModes, values.sensorMode) ||
            !readRequiredBool(object, "h", values.preheatEnabled) ||
            !readOptionalInteger(object, "w",
                                 values.maximumProductWaitMinutes) ||
            !readDouble(object["q"], values.qualificationBandCelsius) ||
            !readInteger(object["qd"], values.qualificationDurationMinutes) ||
            !readInteger(object["tr"], values.maximumTargetReachMinutes) ||
            !readEnum(object["c"], kCompletionModes, values.completionMode) ||
            !readOptionalDouble(object, "k", values.coolingTargetCelsius) ||
            !readOptionalInteger(object, "l", values.holdDurationMinutes)) {
            return false;
        }
        output = FermentationUiStartManualTimedIntent{values};
        return true;
    }

    if (type == "start-manual-holding") {
        if (!hasOnlyKeys(object, {"t", "p"})) return false;
        FermentationUiManualRunPlanValues plan;
        if (!readManualPlan(object["p"], plan)) return false;
        output = FermentationUiStartManualHoldingIntent{std::move(plan)};
        return true;
    }

    if (type == "stop-run") {
        if (!hasOnlyKeys(object, {"t", "o", "p"})) {
            return false;
        }
        StopOption option{};
        if (!readEnum(object["o"], kStopOptions, option)) return false;
        std::optional<FermentationUiManualRunPlanValues> plan;
        if (!object["p"].isUnbound()) {
            FermentationUiManualRunPlanValues parsed;
            if (!readManualPlan(object["p"], parsed)) return false;
            plan = std::move(parsed);
        }
        output = FermentationUiStopRunIntent{option, std::move(plan)};
        return true;
    }

    if (type == "complete-run") {
        if (!hasOnlyKeys(object, {"t", "c", "p"})) {
            return false;
        }
        bool startCooling{};
        if (!readRequiredBool(object, "c", startCooling)) {
            return false;
        }
        std::optional<FermentationUiManualRunPlanValues> plan;
        if (!object["p"].isUnbound()) {
            FermentationUiManualRunPlanValues parsed;
            if (!readManualPlan(object["p"], parsed)) return false;
            plan = std::move(parsed);
        }
        output = FermentationUiCompleteRunIntent{startCooling, std::move(plan)};
        return true;
    }

    if (type == "adjust-run") {
        if (!hasOnlyKeys(object, {"t", "x", "d"})) {
            return false;
        }
        FermentationUiAdjustRunIntent intent;
        if (!readOptionalDouble(object, "x", intent.targetTemperatureCelsius) ||
            !readOptionalInteger(object, "d",
                                 intent.remainingDurationMinutes)) {
            return false;
        }
        output = intent;
        return true;
    }

    if (type == "recovery-time-correction") {
        if (!hasOnlyKeys(object, {"t", "d"})) return false;
        FermentationUiRecoveryTimeCorrectionIntent intent;
        if (!readInteger(object["d"], intent.secondsDelta)) {
            return false;
        }
        output = intent;
        return true;
    }

    if (type == "ack-message" || type == "mute-message") {
        if (!hasOnlyKeys(object, {"t", "id"})) return false;
        std::uint32_t id{};
        if (!readInteger(object["id"], id)) return false;
        output =
            type == "ack-message"
                ? FermentationUiEnvelopePayload{FermentationUiAcknowledgeMessageIntent{
                      id}}
                : FermentationUiEnvelopePayload{
                      FermentationUiMuteMessageIntent{id}};
        return true;
    }

    if (type == "reset-fault") {
        if (!hasOnlyKeys(object, {"t"})) return false;
        output = FermentationUiResetFaultIntent{};
        return true;
    }

    if (type == "sensor-selection") {
        if (!hasOnlyKeys(object, {"t", "a"})) return false;
        SensorSelectionUserAction action{};
        if (!readEnum(object["a"], kSensorActions, action)) return false;
        output = FermentationUiSensorSelectionIntent{action};
        return true;
    }

    return false;
}

template <typename Document>
bool serializeBounded(const Document& document, std::size_t maximumBytes,
                      std::string& output) {
    if (document.overflowed()) return false;
    const auto measured = ArduinoJson::measureJson(document);
    if (measured == 0U || measured > maximumBytes) return false;
    std::string buffer(measured + 1U, '\0');
    const auto written =
        ArduinoJson::serializeJson(document, buffer.data(), buffer.size());
    if (written != measured || written > maximumBytes) return false;
    buffer.resize(written);
    output = std::move(buffer);
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

    ArduinoJson::JsonDocument document;
    const auto error = ArduinoJson::deserializeJson(
        document, exactBody.data(), exactBody.size(),
        ArduinoJson::DeserializationOption::NestingLimit{
            kMaximumWebJsonNesting});
    if (error || document.overflowed() || !document.is<JsonObjectConst>()) {
        return WebRunMutationDecodeStatus::Invalid;
    }
    const auto root = document.as<JsonObjectConst>();
    std::uint32_t version{};
    if (!hasOnlyKeys(root, {"v", "r", "i"}) ||
        !readInteger(root["v"], version) || version != 1U) {
        return WebRunMutationDecodeStatus::Invalid;
    }

    WebRunMutationDto parsed;
    if (!readExpected(root["r"], parsed.expected) ||
        !readIntent(root["i"], parsed.intent) || document.overflowed()) {
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
        networkMode == nullptr)
        return false;

    ArduinoJson::JsonDocument document;
    auto root = document.to<ArduinoJson::JsonObject>();
    root["version"] = 1U;
    root["ready"] = snapshot.status.ready;
    root["homeMode"] = homeMode;
    root["processState"] = processState;
    root["networkMode"] = networkMode;
    root["selectionRequired"] = snapshot.network.selectionRequired;
    auto revisions = root["revisions"].to<ArduinoJson::JsonObject>();
    revisions["stateSequence"] = snapshot.revisions.expectedStateSequence;
    if (snapshot.revisions.expectedRunRevision.has_value())
        revisions["run"] = *snapshot.revisions.expectedRunRevision;
    if (snapshot.revisions.expectedMessageRevision.has_value())
        revisions["messages"] = *snapshot.revisions.expectedMessageRevision;
    if (snapshot.revisions.expectedFaultRevision.has_value())
        revisions["fault"] = *snapshot.revisions.expectedFaultRevision;
    if (snapshot.revisions.expectedRecoveryEpisodeRevision.has_value())
        revisions["recoveryEpisode"] =
            *snapshot.revisions.expectedRecoveryEpisodeRevision;
    if (snapshot.revisions.expectedUserConfigurationRevision.has_value())
        revisions["userConfiguration"] =
            snapshot.revisions.expectedUserConfigurationRevision->value();
    if (snapshot.revisions.expectedProgramCatalogRevision.has_value())
        revisions["programCatalog"] =
            snapshot.revisions.expectedProgramCatalogRevision->value();
    return serializeBounded(document, kMaximumWebApiResponseBodyBytes, output);
}

bool encodeWebApiTemperatures(const FermentationUiSnapshot& snapshot,
                              std::string& output) {
    if (snapshot.temperatures.size() > 3U) return false;
    ArduinoJson::JsonDocument document;
    auto root = document.to<ArduinoJson::JsonObject>();
    root["version"] = 1U;
    auto temperatures = root["temperatures"].to<ArduinoJson::JsonArray>();
    for (const auto& view : snapshot.temperatures) {
        const auto* role = temperatureRoleName(view.role);
        const auto* quality = qualityName(view.quality.quality);
        if (role == nullptr || quality == nullptr) return false;
        const bool valid =
            view.quality.quality == device_platform::SensorQuality::Valid &&
            view.valueCelsius.has_value() && std::isfinite(*view.valueCelsius);
        auto item = temperatures.add<ArduinoJson::JsonObject>();
        item["role"] = role;
        item["quality"] = quality;
        item["valid"] = valid;
        if (valid)
            item["valueCelsius"] = *view.valueCelsius;
        else
            item["valueCelsius"] = nullptr;
    }
    return serializeBounded(document, kMaximumWebApiResponseBodyBytes, output);
}

bool encodeWebApiAlerts(const FermentationUiSnapshot& snapshot,
                        std::string& output) {
    if (snapshot.messages.size() > kMaximumWebApiAlertCount) return false;
    ArduinoJson::JsonDocument document;
    auto root = document.to<ArduinoJson::JsonObject>();
    root["version"] = 1U;
    auto alerts = root["alerts"].to<ArduinoJson::JsonArray>();
    for (const auto& view : snapshot.messages) {
        const auto& message = view.message;
        const auto* code = messageCodeName(message.code);
        const auto* severity = messageSeverityName(message.messageClass);
        if (code == nullptr || severity == nullptr) return false;
        auto item = alerts.add<ArduinoJson::JsonObject>();
        item["id"] = message.id;
        item["code"] = code;
        item["severity"] = severity;
        item["active"] = message.active;
        item["acknowledged"] = message.acknowledged;
        item["resolved"] = message.resolved;
        item["decisionRequired"] = message.decisionRequired;
        item["muted"] = message.acousticMuted;
        item["revision"] = message.revision;
    }
    return serializeBounded(document, kMaximumWebApiResponseBodyBytes, output);
}

}  // namespace fermentation
