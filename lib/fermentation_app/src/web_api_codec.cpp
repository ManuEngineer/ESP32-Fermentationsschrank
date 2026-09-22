#include "web_api_codec.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <ArduinoJson.h>

namespace fermentation {
namespace {

constexpr std::size_t kMaximumJsonResponseBytes = 4096U;
constexpr std::size_t kMaximumJsonRequestBytes = 4096U;
constexpr std::size_t kMaximumProgramIdBytes = 128U;

using JsonDocument = ArduinoJson::JsonDocument;

WebApiCodecStatus requiredString(const JsonDocument& document, const char* field,
                                 std::size_t maximumBytes,
                                 std::string& out) {
    const auto value = document[field];
    if (value.isNull()) return WebApiCodecStatus::MissingField;
    if (!value.is<const char*>()) return WebApiCodecStatus::WrongType;
    const char* text = value.as<const char*>();
    if (text == nullptr) return WebApiCodecStatus::WrongType;
    out.assign(text);
    if (out.empty()) {
        out.clear();
        return WebApiCodecStatus::MissingField;
    }
    if (out.size() > maximumBytes) {
        out.clear();
        return WebApiCodecStatus::CapacityExceeded;
    }
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus requiredUInt32(const JsonDocument& document, const char* field,
                                 std::uint32_t& out) {
    const auto value = document[field];
    if (value.isNull()) return WebApiCodecStatus::MissingField;
    if (!value.is<std::uint64_t>() ||
        value.as<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max()) {
        return WebApiCodecStatus::WrongType;
    }
    out = value.as<std::uint32_t>();
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus optionalUInt32(
    const JsonDocument& document, const char* field,
    std::optional<std::uint32_t>& out) {
    const auto value = document[field];
    if (value.isNull()) {
        out.reset();
        return WebApiCodecStatus::Success;
    }
    std::uint32_t parsed = 0U;
    const auto status = requiredUInt32(document, field, parsed);
    if (status == WebApiCodecStatus::Success) out = parsed;
    return status;
}

WebApiCodecStatus requiredDouble(const JsonDocument& document, const char* field,
                                 double& out) {
    const auto value = document[field];
    if (value.isNull()) return WebApiCodecStatus::MissingField;
    if (!value.is<double>()) return WebApiCodecStatus::WrongType;
    const double parsed = value.as<double>();
    if (!std::isfinite(parsed)) return WebApiCodecStatus::WrongType;
    out = parsed;
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus optionalDouble(const JsonDocument& document, const char* field,
                                 std::optional<double>& out) {
    const auto value = document[field];
    if (value.isNull()) {
        out.reset();
        return WebApiCodecStatus::Success;
    }
    double parsed = 0.0;
    const auto status = requiredDouble(document, field, parsed);
    if (status == WebApiCodecStatus::Success) out = parsed;
    return status;
}

WebApiCodecStatus optionalBoolean(const JsonDocument& document, const char* field,
                                  std::optional<bool>& out) {
    const auto value = document[field];
    if (value.isNull()) {
        out.reset();
        return WebApiCodecStatus::Success;
    }
    if (!value.is<bool>()) return WebApiCodecStatus::WrongType;
    out = value.as<bool>();
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus requiredBoolean(const JsonDocument& document, const char* field,
                                  bool& out) {
    const auto value = document[field];
    if (value.isNull()) return WebApiCodecStatus::MissingField;
    if (!value.is<bool>()) return WebApiCodecStatus::WrongType;
    out = value.as<bool>();
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus optionalRevision(const JsonDocument& document, const char* field,
                                   std::optional<std::uint32_t>& out) {
    return optionalUInt32(document, field, out);
}

WebApiCodecStatus optionalUserRevision(
    const JsonDocument& document, const char* field,
    std::optional<UserConfigurationRevision>& out) {
    const auto value = document[field];
    if (value.isNull()) {
        out.reset();
        return WebApiCodecStatus::Success;
    }
    if (!value.is<std::uint64_t>() || value.as<std::uint64_t>() == 0U) {
        return WebApiCodecStatus::WrongType;
    }
    out = UserConfigurationRevision{value.as<std::uint64_t>()};
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus optionalProgramRevision(
    const JsonDocument& document, const char* field,
    std::optional<ProgramCatalogRevision>& out) {
    const auto value = document[field];
    if (value.isNull()) {
        out.reset();
        return WebApiCodecStatus::Success;
    }
    if (!value.is<std::uint64_t>() || value.as<std::uint64_t>() == 0U) {
        return WebApiCodecStatus::WrongType;
    }
    out = ProgramCatalogRevision{value.as<std::uint64_t>()};
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus sensorMode(const JsonDocument& document, const char* field,
                             RunSensorMode& out, bool required) {
    const auto value = document[field];
    if (value.isNull()) {
        return required ? WebApiCodecStatus::MissingField
                        : WebApiCodecStatus::Success;
    }
    if (!value.is<const char*>()) return WebApiCodecStatus::WrongType;
    const std::string code = value.as<const char*>();
    if (code == "PRODUCT") {
        out = RunSensorMode::Product;
        return WebApiCodecStatus::Success;
    }
    if (code == "AIR") {
        out = RunSensorMode::Air;
        return WebApiCodecStatus::Success;
    }
    return WebApiCodecStatus::WrongType;
}

WebApiCodecStatus completionMode(const JsonDocument& document, const char* field,
                                 CompletionMode& out) {
    const auto value = document[field];
    if (value.isNull()) return WebApiCodecStatus::Success;
    if (!value.is<const char*>()) return WebApiCodecStatus::WrongType;
    const std::string code = value.as<const char*>();
    if (code == "FINISH_WITHOUT_COOLING") {
        out = CompletionMode::FinishWithoutCooling;
    } else if (code == "COOL_THEN_FINISH") {
        out = CompletionMode::CoolThenFinish;
    } else if (code == "COOL_AND_HOLD_FOR_DURATION") {
        out = CompletionMode::CoolAndHoldForDuration;
    } else if (code == "COOL_AND_HOLD_UNTIL_MANUAL_STOP") {
        out = CompletionMode::CoolAndHoldUntilManualStop;
    } else {
        return WebApiCodecStatus::WrongType;
    }
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus decodeManualPlan(const JsonDocument& document,
                                   FermentationUiManualRunPlanValues& out) {
    auto status = requiredDouble(document, "targetTemperatureCelsius",
                                 out.targetTemperatureCelsius);
    if (status != WebApiCodecStatus::Success) return status;
    status = sensorMode(document, "sensorMode", out.sensorMode, false);
    if (status != WebApiCodecStatus::Success) return status;
    std::optional<bool> preheat;
    status = optionalBoolean(document, "preheatEnabled", preheat);
    if (status != WebApiCodecStatus::Success) return status;
    if (preheat.has_value()) out.preheatEnabled = *preheat;
    status = optionalUInt32(document, "maximumProductWaitMinutes",
                            out.maximumProductWaitMinutes);
    if (status != WebApiCodecStatus::Success) return status;
    if (document["qualificationBandCelsius"].isNull()) {
        out.qualificationBandCelsius = 0.0;
    } else {
        status = requiredDouble(document, "qualificationBandCelsius",
                                out.qualificationBandCelsius);
        if (status != WebApiCodecStatus::Success) return status;
    }
    if (document["qualificationDurationMinutes"].isNull()) {
        out.qualificationDurationMinutes = 0U;
    } else {
        status = requiredUInt32(document, "qualificationDurationMinutes",
                                out.qualificationDurationMinutes);
        if (status != WebApiCodecStatus::Success) return status;
    }
    if (document["maximumTargetReachMinutes"].isNull()) {
        out.maximumTargetReachMinutes = 0U;
    } else {
        status = requiredUInt32(document, "maximumTargetReachMinutes",
                                out.maximumTargetReachMinutes);
        if (status != WebApiCodecStatus::Success) return status;
    }
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus decodeExpectedRevisions(const JsonDocument& document,
                                          FermentationUiExpectedRevisions& out) {
    auto status = requiredUInt32(document, "expectedStateSequence",
                                 out.expectedStateSequence);
    if (status != WebApiCodecStatus::Success) return status;
    status = optionalRevision(document, "expectedRunRevision",
                              out.expectedRunRevision);
    if (status != WebApiCodecStatus::Success) return status;
    status = optionalRevision(document, "expectedMessageRevision",
                              out.expectedMessageRevision);
    if (status != WebApiCodecStatus::Success) return status;
    status = optionalRevision(document, "expectedFaultRevision",
                              out.expectedFaultRevision);
    if (status != WebApiCodecStatus::Success) return status;
    status = optionalRevision(document, "expectedRecoveryEpisodeRevision",
                              out.expectedRecoveryEpisodeRevision);
    if (status != WebApiCodecStatus::Success) return status;
    status = optionalUserRevision(document, "expectedUserConfigurationRevision",
                                  out.expectedUserConfigurationRevision);
    if (status != WebApiCodecStatus::Success) return status;
    return optionalProgramRevision(document, "expectedProgramCatalogRevision",
                                   out.expectedProgramCatalogRevision);
}

WebApiCodecStatus requiredAction(const JsonDocument& document,
                                 std::string& out) {
    return requiredString(document, "action", 64U, out);
}

WebApiCodecStatus decodeRunPayload(const JsonDocument& document,
                                   const std::string& action,
                                   FermentationUiEnvelopePayload& out) {
    if (action == "start_program") {
        FermentationUiStartProgramIntent intent;
        auto status = requiredString(document, "programId", kMaximumProgramIdBytes,
                                     intent.candidate.programId);
        if (status != WebApiCodecStatus::Success) return status;
        status = optionalDouble(document, "targetTemperatureCelsius",
                                intent.candidate.targetTemperatureCelsius);
        if (status != WebApiCodecStatus::Success) return status;
        status = optionalUInt32(document, "fermentationDurationMinutes",
                                intent.candidate.fermentationDurationMinutes);
        if (status != WebApiCodecStatus::Success) return status;
        status = optionalBoolean(document, "preheatEnabled",
                                 intent.candidate.preheatEnabled);
        if (status != WebApiCodecStatus::Success) return status;
        RunSensorMode sensor = RunSensorMode::Air;
        status = sensorMode(document, "sensorMode", sensor, false);
        if (status != WebApiCodecStatus::Success) return status;
        if (!document["sensorMode"].isNull()) intent.candidate.sensorMode = sensor;
        CompletionMode completion = CompletionMode::FinishWithoutCooling;
        status = completionMode(document, "completionMode", completion);
        if (status != WebApiCodecStatus::Success) return status;
        if (!document["completionMode"].isNull()) {
            intent.candidate.completionMode = completion;
        }
        status = optionalDouble(document, "coolingTargetCelsius",
                                intent.candidate.coolingTargetCelsius);
        if (status != WebApiCodecStatus::Success) return status;
        status = optionalUInt32(document, "holdDurationMinutes",
                                intent.candidate.holdDurationMinutes);
        if (status != WebApiCodecStatus::Success) return status;
        out = std::move(intent);
        return WebApiCodecStatus::Success;
    }
    if (action == "start_manual_holding") {
        FermentationUiStartManualHoldingIntent intent;
        const auto status = decodeManualPlan(document, intent.plan);
        if (status != WebApiCodecStatus::Success) return status;
        out = std::move(intent);
        return WebApiCodecStatus::Success;
    }
    if (action == "start_manual_timed") {
        FermentationUiStartManualTimedIntent intent;
        auto status = requiredDouble(document, "targetTemperatureCelsius",
                                     intent.values.targetTemperatureCelsius);
        if (status != WebApiCodecStatus::Success) return status;
        status = requiredUInt32(document, "durationMinutes",
                                intent.values.durationMinutes);
        if (status != WebApiCodecStatus::Success) return status;
        status = sensorMode(document, "sensorMode", intent.values.sensorMode, false);
        if (status != WebApiCodecStatus::Success) return status;
        std::optional<bool> preheat;
        status = optionalBoolean(document, "preheatEnabled", preheat);
        if (status != WebApiCodecStatus::Success) return status;
        if (preheat.has_value()) intent.values.preheatEnabled = *preheat;
        status = optionalUInt32(document, "maximumProductWaitMinutes",
                                intent.values.maximumProductWaitMinutes);
        if (status != WebApiCodecStatus::Success) return status;
        status = requiredDouble(document, "qualificationBandCelsius",
                                intent.values.qualificationBandCelsius);
        if (status != WebApiCodecStatus::Success) return status;
        status = requiredUInt32(document, "qualificationDurationMinutes",
                                intent.values.qualificationDurationMinutes);
        if (status != WebApiCodecStatus::Success) return status;
        status = requiredUInt32(document, "maximumTargetReachMinutes",
                                intent.values.maximumTargetReachMinutes);
        if (status != WebApiCodecStatus::Success) return status;
        status = completionMode(document, "completionMode",
                                intent.values.completionMode);
        if (status != WebApiCodecStatus::Success) return status;
        status = optionalDouble(document, "coolingTargetCelsius",
                                intent.values.coolingTargetCelsius);
        if (status != WebApiCodecStatus::Success) return status;
        status = optionalUInt32(document, "holdDurationMinutes",
                                intent.values.holdDurationMinutes);
        if (status != WebApiCodecStatus::Success) return status;
        out = std::move(intent);
        return WebApiCodecStatus::Success;
    }
    if (action == "stop") {
        FermentationUiStopRunIntent intent;
        std::string option;
        auto status = requiredString(document, "option", 32U, option);
        if (status != WebApiCodecStatus::Success) return status;
        if (option == "back") {
            intent.option = StopOption::Back;
        } else if (option == "abort_and_turn_off") {
            intent.option = StopOption::AbortAndTurnOff;
        } else if (option == "abort_and_cool") {
            intent.option = StopOption::AbortAndCool;
        } else {
            return WebApiCodecStatus::WrongType;
        }
        if (intent.option == StopOption::AbortAndCool) {
            FermentationUiManualRunPlanValues plan;
            status = decodeManualPlan(document, plan);
            if (status != WebApiCodecStatus::Success) return status;
            intent.coolingPlan = plan;
        }
        out = std::move(intent);
        return WebApiCodecStatus::Success;
    }
    if (action == "complete") {
        FermentationUiCompleteRunIntent intent;
        auto status = requiredBoolean(document, "startCooling", intent.startCooling);
        if (status != WebApiCodecStatus::Success) return status;
        if (intent.startCooling) {
            FermentationUiManualRunPlanValues plan;
            status = decodeManualPlan(document, plan);
            if (status != WebApiCodecStatus::Success) return status;
            intent.coolingPlan = plan;
        }
        out = std::move(intent);
        return WebApiCodecStatus::Success;
    }
    if (action == "adjust") {
        FermentationUiAdjustRunIntent intent;
        auto status = optionalDouble(document, "targetTemperatureCelsius",
                                     intent.targetTemperatureCelsius);
        if (status != WebApiCodecStatus::Success) return status;
        status = optionalUInt32(document, "remainingDurationMinutes",
                                intent.remainingDurationMinutes);
        if (status != WebApiCodecStatus::Success) return status;
        if (!intent.targetTemperatureCelsius.has_value() &&
            !intent.remainingDurationMinutes.has_value()) {
            return WebApiCodecStatus::MissingField;
        }
        out = std::move(intent);
        return WebApiCodecStatus::Success;
    }
    if (action == "recovery_time") {
        FermentationUiRecoveryTimeCorrectionIntent intent;
        const auto status = requiredUInt32(document, "secondsDelta", intent.secondsDelta);
        if (status != WebApiCodecStatus::Success) return status;
        out = intent;
        return WebApiCodecStatus::Success;
    }
    if (action == "acknowledge" || action == "mute") {
        std::uint32_t messageId = 0U;
        const auto status = requiredUInt32(document, "messageId", messageId);
        if (status != WebApiCodecStatus::Success) return status;
        if (action == "acknowledge") {
            out = FermentationUiAcknowledgeMessageIntent{messageId};
        } else {
            out = FermentationUiMuteMessageIntent{messageId};
        }
        return WebApiCodecStatus::Success;
    }
    if (action == "sensor_selection") {
        FermentationUiSensorSelectionIntent intent;
        std::string selection;
        const auto status = requiredString(document, "selection", 48U, selection);
        if (status != WebApiCodecStatus::Success) return status;
        if (selection == "continue_with_air") {
            intent.action = SensorSelectionUserAction::ContinueWithAir;
        } else if (selection == "return_to_product") {
            intent.action = SensorSelectionUserAction::ReturnToProduct;
        } else if (selection == "recheck_product") {
            intent.action = SensorSelectionUserAction::RecheckProduct;
        } else {
            return WebApiCodecStatus::WrongType;
        }
        out = intent;
        return WebApiCodecStatus::Success;
    }
    if (action == "reset_fault") {
        out = FermentationUiResetFaultIntent{};
        return WebApiCodecStatus::Success;
    }
    return WebApiCodecStatus::WrongType;
}

bool finish(ArduinoJson::JsonDocument& document, std::string& out) {
    out.clear();
    if (measureJson(document) > kMaximumJsonResponseBytes) return false;
    out.reserve(measureJson(document));
    serializeJson(document, out);
    return out.size() <= kMaximumJsonResponseBytes;
}

const char* networkModeCode(device_platform::NetworkMode mode) {
    switch (mode) {
        case device_platform::NetworkMode::UNSELECTED:
            return "UNSELECTED";
        case device_platform::NetworkMode::AP_ONLY:
            return "AP_ONLY";
        case device_platform::NetworkMode::HOME_WIFI:
            return "HOME_WIFI";
    }
    return "UNKNOWN";
}

}  // namespace

WebApiCodecStatus encodeUiSnapshot(const FermentationUiSnapshot& snapshot,
                                   std::string& out) {
    return encodeUiSnapshot(snapshot,
                            MutationSequenceView{MutationSequenceState::Exhausted,
                                                 0U, std::nullopt},
                            std::nullopt, out);
}

WebApiCodecStatus encodeUiSnapshot(const FermentationUiSnapshot& snapshot,
                                   const MutationSequenceView& sequence,
                                   std::string& out) {
    return encodeUiSnapshot(snapshot, sequence, std::nullopt, out);
}

WebApiCodecStatus encodeUiSnapshot(const FermentationUiSnapshot& snapshot,
                                   const MutationSequenceView& sequence,
                                   std::optional<bool> webPasswordEnabled,
                                   std::string& out) {
    return encodeUiSnapshot(snapshot, sequence, webPasswordEnabled,
                            std::nullopt, out);
}

WebApiCodecStatus encodeUiSnapshot(const FermentationUiSnapshot& snapshot,
                                   const MutationSequenceView& sequence,
                                   std::optional<bool> webPasswordEnabled,
                                   std::optional<std::string> csrfToken,
                                   std::string& out) {
    ArduinoJson::JsonDocument document;
    auto root = document.to<ArduinoJson::JsonObject>();
    root["refreshRevision"] = snapshot.refreshRevision.has_value()
                                  ? snapshot.refreshRevision->value
                                  : 0U;
    root["ready"] = snapshot.status.ready;
    if (webPasswordEnabled.has_value()) {
        root["webPasswordEnabled"] = *webPasswordEnabled;
    }
    if (csrfToken.has_value() && csrfToken->size() == 32U) {
        root["csrfToken"] = *csrfToken;
    }
    root["networkMode"] = networkModeCode(snapshot.network.currentMode);
    root["selectionRequired"] = snapshot.network.selectionRequired;
    auto selectableModes = root["selectableModes"].to<ArduinoJson::JsonArray>();
    for (const auto mode : snapshot.network.selectableModes) {
        selectableModes.add(networkModeCode(mode));
    }
    if (snapshot.revisions.expectedUserConfigurationRevision.has_value()) {
        root["expectedUserConfigurationRevision"] =
            snapshot.revisions.expectedUserConfigurationRevision->value();
    }
    const char* sequenceState =
        sequence.state == MutationSequenceState::Available
            ? "AVAILABLE"
            : sequence.state == MutationSequenceState::InFlight ? "IN_FLIGHT"
                                                                 : "EXHAUSTED";
    root["nextMutationSeq"] = sequence.nextMutationSeq;
    root["mutationSequenceState"] = sequenceState;
    if (sequence.inFlightMutationSeq.has_value()) {
        root["inFlightMutationSeq"] = *sequence.inFlightMutationSeq;
    }
    root["homeMode"] = static_cast<std::uint8_t>(snapshot.home.mode);
    root["processState"] = static_cast<std::uint8_t>(snapshot.home.processState);
    root["activeRunId"] = snapshot.home.activeRunId;
    auto temperatures = root["temperatures"].to<ArduinoJson::JsonArray>();
    for (const auto& temperature : snapshot.temperatures) {
        auto item = temperatures.add<ArduinoJson::JsonObject>();
        item["role"] = static_cast<std::uint8_t>(temperature.role);
        if (temperature.valueCelsius.has_value())
            item["celsius"] = *temperature.valueCelsius;
        else
            item["celsius"] = nullptr;
        item["quality"] = static_cast<std::uint8_t>(temperature.quality.quality);
    }
    return finish(document, out) ? WebApiCodecStatus::Success
                                 : WebApiCodecStatus::CapacityExceeded;
}

WebApiCodecStatus encodeStatus(const FermentationUiSnapshot& snapshot,
                               std::string& out) {
    return encodeStatus(snapshot, std::nullopt, out);
}

WebApiCodecStatus encodeStatus(const FermentationUiSnapshot& snapshot,
                               std::optional<bool> webPasswordEnabled,
                               std::string& out) {
    ArduinoJson::JsonDocument document;
    auto root = document.to<ArduinoJson::JsonObject>();
    root["ready"] = snapshot.status.ready;
    if (webPasswordEnabled.has_value()) {
        root["webPasswordEnabled"] = *webPasswordEnabled;
    }
    root["networkMode"] = networkModeCode(snapshot.network.currentMode);
    root["homeMode"] = static_cast<std::uint8_t>(snapshot.home.mode);
    root["processState"] = static_cast<std::uint8_t>(snapshot.home.processState);
    root["activeRunId"] = snapshot.home.activeRunId;
    return finish(document, out) ? WebApiCodecStatus::Success
                                 : WebApiCodecStatus::CapacityExceeded;
}

WebApiCodecStatus encodeTemperatures(const FermentationUiSnapshot& snapshot,
                                     std::string& out) {
    ArduinoJson::JsonDocument document;
    auto temperatures = document["temperatures"].to<ArduinoJson::JsonArray>();
    for (const auto& temperature : snapshot.temperatures) {
        auto item = temperatures.add<ArduinoJson::JsonObject>();
        item["role"] = static_cast<std::uint8_t>(temperature.role);
        if (temperature.valueCelsius.has_value())
            item["celsius"] = *temperature.valueCelsius;
        item["quality"] = static_cast<std::uint8_t>(temperature.quality.quality);
    }
    return finish(document, out) ? WebApiCodecStatus::Success
                                 : WebApiCodecStatus::CapacityExceeded;
}

WebApiCodecStatus encodeAlerts(const FermentationUiSnapshot& snapshot,
                               std::string& out) {
    ArduinoJson::JsonDocument document;
    auto alerts = document["alerts"].to<ArduinoJson::JsonArray>();
    for (const auto& view : snapshot.messages) {
        auto item = alerts.add<ArduinoJson::JsonObject>();
        item["id"] = view.message.id;
        item["active"] = view.message.active;
        item["resolved"] = view.message.resolved;
        item["decisionRequired"] = view.message.decisionRequired;
        item["code"] = static_cast<std::uint8_t>(view.message.code);
    }
    return finish(document, out) ? WebApiCodecStatus::Success
                                 : WebApiCodecStatus::CapacityExceeded;
}

WebApiCodecStatus encodeSessionHandoff(const std::string& csrfToken,
                                       const MutationSequenceView& sequence,
                                       std::string& out) {
    if (csrfToken.size() != 32U) return WebApiCodecStatus::WrongType;
    ArduinoJson::JsonDocument document;
    document["csrfToken"] = csrfToken;
    document["nextMutationSeq"] = sequence.nextMutationSeq;
    document["mutationSequenceState"] =
        sequence.state == MutationSequenceState::Available
            ? "AVAILABLE"
            : sequence.state == MutationSequenceState::InFlight ? "IN_FLIGHT"
                                                                 : "EXHAUSTED";
    if (sequence.inFlightMutationSeq.has_value()) {
        document["inFlightMutationSeq"] = *sequence.inFlightMutationSeq;
    }
    return finish(document, out) ? WebApiCodecStatus::Success
                                 : WebApiCodecStatus::CapacityExceeded;
}

WebApiCodecStatus encodeError(const char* code, const char* message,
                              std::string& out) {
    if (code == nullptr || message == nullptr) return WebApiCodecStatus::WrongType;
    ArduinoJson::JsonDocument document;
    document["code"] = code;
    document["message"] = message;
    return finish(document, out) ? WebApiCodecStatus::Success
                                 : WebApiCodecStatus::CapacityExceeded;
}

WebApiCodecStatus decodeCredentialField(const std::string& body,
                                        const char* field,
                                        std::size_t maximumBytes,
                                        std::string& out) {
    if (body.size() > 4096U || field == nullptr) return WebApiCodecStatus::CapacityExceeded;
    ArduinoJson::JsonDocument document;
    const auto error = deserializeJson(document, body);
    if (error) return WebApiCodecStatus::InvalidJson;
    const auto value = document[field];
    if (value.isNull()) return WebApiCodecStatus::MissingField;
    if (!value.is<const char*>()) return WebApiCodecStatus::WrongType;
    const char* text = value.as<const char*>();
    if (text == nullptr) return WebApiCodecStatus::WrongType;
    out.assign(text);
    if (out.size() > maximumBytes) {
        out.clear();
        return WebApiCodecStatus::CapacityExceeded;
    }
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus decodeBooleanField(const std::string& body, const char* field,
                                     bool& out) {
    if (body.size() > 4096U || field == nullptr) {
        return WebApiCodecStatus::CapacityExceeded;
    }
    ArduinoJson::JsonDocument document;
    if (deserializeJson(document, body)) return WebApiCodecStatus::InvalidJson;
    const auto value = document[field];
    if (value.isNull()) return WebApiCodecStatus::MissingField;
    if (!value.is<bool>()) return WebApiCodecStatus::WrongType;
    out = value.as<bool>();
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus decodeNetworkMode(const std::string& body,
                                    device_platform::NetworkMode& out,
                                    std::optional<UserConfigurationRevision>&
                                        expectedRevision) {
    if (body.size() > 1024U) return WebApiCodecStatus::CapacityExceeded;
    ArduinoJson::JsonDocument document;
    if (deserializeJson(document, body)) return WebApiCodecStatus::InvalidJson;
    const auto value = document["mode"];
    if (!value.is<const char*>()) return WebApiCodecStatus::MissingField;
    const std::string mode = value.as<const char*>();
    if (mode == "AP_ONLY") {
        out = device_platform::NetworkMode::AP_ONLY;
        return WebApiCodecStatus::Success;
    }
    if (mode == "HOME_WIFI") {
        out = device_platform::NetworkMode::HOME_WIFI;
    } else {
        return WebApiCodecStatus::WrongType;
    }
    const auto revision = document["expectedUserConfigurationRevision"];
    if (!revision.isNull()) {
        if (!revision.is<std::uint64_t>() || revision.as<std::uint64_t>() == 0U) {
            return WebApiCodecStatus::WrongType;
        }
        expectedRevision = UserConfigurationRevision{revision.as<std::uint64_t>()};
    } else {
        expectedRevision.reset();
    }
    return WebApiCodecStatus::Success;
}

WebApiCodecStatus decodeWebUiRunCommand(const std::string& body,
                                        WebUiRunCommand& out) {
    if (body.size() > kMaximumJsonRequestBytes) {
        return WebApiCodecStatus::CapacityExceeded;
    }
    JsonDocument document;
    if (deserializeJson(document, body)) {
        return WebApiCodecStatus::InvalidJson;
    }

    auto status = decodeExpectedRevisions(document, out.expected);
    if (status != WebApiCodecStatus::Success) return status;
    status = requiredBoolean(document, "confirmed", out.confirmed);
    if (status != WebApiCodecStatus::Success) return status;
    std::string action;
    status = requiredAction(document, action);
    if (status != WebApiCodecStatus::Success) return status;
    return decodeRunPayload(document, action, out.payload);
}

}  // namespace fermentation
