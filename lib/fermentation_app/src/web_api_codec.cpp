#include "web_api_codec.hpp"

#include <algorithm>

#include <ArduinoJson.h>

namespace fermentation {
namespace {

constexpr std::size_t kMaximumJsonResponseBytes = 4096U;

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

}  // namespace fermentation
