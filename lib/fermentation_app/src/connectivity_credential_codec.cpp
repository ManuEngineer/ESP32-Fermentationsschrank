#include "connectivity_credential_codec.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "configuration_limits.hpp"

namespace fermentation {
namespace {

bool writeString(device_platform::ByteWriter& writer,
                 const std::string& value) {
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }
    return device_platform::big_endian::writeUint16(
               writer, static_cast<std::uint16_t>(value.size())) &&
           writer.writeBytes(value.data(), value.size());
}

bool readString(device_platform::ByteReader& reader, std::size_t maximumBytes,
                std::string& out) {
    std::uint16_t length = 0U;
    if (!device_platform::big_endian::readUint16(reader, length) ||
        length > maximumBytes || length > reader.remaining()) {
        return false;
    }
    std::string value(length, '\0');
    if (!reader.readBytes(value.data(), length)) {
        return false;
    }
    out = std::move(value);
    return true;
}

}  // namespace

ConnectivityCredentialValidationStatus validateConnectivityCredential(
    const ConnectivityCredential& credential) {
    if (!credential.homeWifi.has_value()) {
        return ConnectivityCredentialValidationStatus::Success;
    }
    const auto& values = *credential.homeWifi;
    if (values.ssid.empty() ||
        values.ssid.size() > configuration_limits::kMaximumHomeWifiSsidBytes ||
        std::any_of(values.ssid.begin(), values.ssid.end(),
                    [](char value) { return value == '\0'; })) {
        return ConnectivityCredentialValidationStatus::InvalidSsid;
    }
    if (values.password.size() <
            configuration_limits::kMinimumHomeWifiPasswordBytes ||
        values.password.size() >
            configuration_limits::kMaximumHomeWifiPasswordBytes ||
        std::any_of(values.password.begin(), values.password.end(),
                    [](char value) {
                        const auto byte = static_cast<unsigned char>(value);
                        return byte < 0x20U || byte == 0x7FU;
                    })) {
        return ConnectivityCredentialValidationStatus::InvalidPassword;
    }
    return ConnectivityCredentialValidationStatus::Success;
}

ConnectivityCredentialCodecStatus encodeConnectivityCredentialPayload(
    const ConnectivityCredential& credential, std::string& out) {
    if (validateConnectivityCredential(credential) !=
        ConnectivityCredentialValidationStatus::Success) {
        return ConnectivityCredentialCodecStatus::InvalidCredential;
    }
    device_platform::ByteWriter writer(
        configuration_limits::kMaximumConnectivityCredentialPayloadBytes);
    const bool hasHomeWifi = credential.homeWifi.has_value();
    if (!device_platform::big_endian::writeOptionalTag(writer, hasHomeWifi) ||
        (hasHomeWifi &&
         (!writeString(writer, credential.homeWifi->ssid) ||
          !writeString(writer, credential.homeWifi->password)))) {
        return ConnectivityCredentialCodecStatus::CapacityExceeded;
    }
    out = writer.takeBytes();
    return ConnectivityCredentialCodecStatus::Success;
}

ConnectivityCredentialDecodeResult decodeConnectivityCredentialPayload(
    std::uint32_t schemaVersion, const std::string& payload) {
    if (schemaVersion != 1U) {
        return {ConnectivityCredentialCodecStatus::UnsupportedSchema,
                std::nullopt};
    }
    if (payload.size() >
        configuration_limits::kMaximumConnectivityCredentialPayloadBytes) {
        return {ConnectivityCredentialCodecStatus::CapacityExceeded,
                std::nullopt};
    }
    device_platform::ByteReader reader(payload);
    bool hasHomeWifi = false;
    if (!device_platform::big_endian::readOptionalTag(reader, hasHomeWifi)) {
        return {ConnectivityCredentialCodecStatus::InvalidWireValue,
                std::nullopt};
    }
    ConnectivityCredential candidate;
    if (hasHomeWifi) {
        device_platform::NetworkCredentials values;
        if (!readString(reader, configuration_limits::kMaximumHomeWifiSsidBytes,
                        values.ssid) ||
            !readString(reader,
                        configuration_limits::kMaximumHomeWifiPasswordBytes,
                        values.password)) {
            return {ConnectivityCredentialCodecStatus::Truncated, std::nullopt};
        }
        candidate.homeWifi = std::move(values);
    }
    if (reader.remaining() != 0U) {
        return {ConnectivityCredentialCodecStatus::TrailingBytes, std::nullopt};
    }
    if (validateConnectivityCredential(candidate) !=
        ConnectivityCredentialValidationStatus::Success) {
        return {ConnectivityCredentialCodecStatus::InvalidCredential,
                std::nullopt};
    }
    return {ConnectivityCredentialCodecStatus::Success, std::move(candidate)};
}

}  // namespace fermentation
