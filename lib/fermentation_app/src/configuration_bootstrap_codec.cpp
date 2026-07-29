#include "configuration_bootstrap_codec.hpp"

#include <utility>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "storage_envelope.hpp"

namespace fermentation {

ConfigurationBootstrapCodecStatus encodeConfigurationBootstrapRecord(
    const ConfigurationBootstrapRecord& record, std::string& out) {
    if (!isPlausible(record)) {
        return ConfigurationBootstrapCodecStatus::InvalidModel;
    }
    device_platform::ByteWriter payload(
        configuration_limits::kConfigurationBootstrapPayloadBytes);
    if (!device_platform::big_endian::writeUint32(
            payload, record.storageFormatVersion.value()) ||
        !device_platform::big_endian::writeUint8(
            payload, static_cast<std::uint8_t>(record.state))) {
        return ConfigurationBootstrapCodecStatus::CapacityExceeded;
    }
    const device_platform::StorageEnvelope envelope{
        configuration_storage_contract::kConfigurationBootstrapRecordType,
        kConfigurationBootstrapSchemaVersion1,
        record.storageEpoch,
        record.sequence.value(),
        std::nullopt,
        payload.takeBytes()};
    std::string encoded;
    const auto status = device_platform::encodeEnvelope(
        envelope, encoded,
        configuration_limits::kMaximumConfigurationBootstrapEnvelopeBytes);
    if (status == device_platform::EnvelopeEncodeStatus::CapacityExceeded) {
        return ConfigurationBootstrapCodecStatus::CapacityExceeded;
    }
    if (status != device_platform::EnvelopeEncodeStatus::Success) {
        return ConfigurationBootstrapCodecStatus::InvalidModel;
    }
    out.swap(encoded);
    return ConfigurationBootstrapCodecStatus::Success;
}

ConfigurationBootstrapDecodeResult decodeConfigurationBootstrapRecord(
    const std::string& bytes) {
    const auto decoded = device_platform::decodeEnvelope(bytes);
    if (decoded.status ==
        device_platform::EnvelopeDecodeStatus::UnknownEnvelopeVersion) {
        return {ConfigurationBootstrapCodecStatus::UnsupportedNewerSchema,
                std::nullopt};
    }
    if (decoded.status != device_platform::EnvelopeDecodeStatus::Success ||
        !decoded.envelope.has_value()) {
        return {ConfigurationBootstrapCodecStatus::InvalidEnvelope,
                std::nullopt};
    }
    const auto& envelope = *decoded.envelope;
    if (envelope.recordTypeId !=
        configuration_storage_contract::kConfigurationBootstrapRecordType) {
        return {ConfigurationBootstrapCodecStatus::RecordIdentityMismatch,
                std::nullopt};
    }
    if (envelope.schemaVersion != kConfigurationBootstrapSchemaVersion1) {
        return {envelope.schemaVersion > kConfigurationBootstrapSchemaVersion1
                    ? ConfigurationBootstrapCodecStatus::UnsupportedNewerSchema
                    : ConfigurationBootstrapCodecStatus::InvalidModel,
                std::nullopt};
    }
    if (envelope.utcUnixSeconds.has_value() ||
        envelope.payload.size() !=
            configuration_limits::kConfigurationBootstrapPayloadBytes) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }
    device_platform::ByteReader reader(envelope.payload);
    std::uint32_t format = 0U;
    std::uint8_t state = 0U;
    if (!device_platform::big_endian::readUint32(reader, format) ||
        !device_platform::big_endian::readUint8(reader, state) ||
        reader.remaining() != 0U) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }
    if (format > kConfigurationStorageFormatVersion1.value()) {
        return {ConfigurationBootstrapCodecStatus::UnsupportedNewerSchema,
                std::nullopt};
    }
    const ConfigurationBootstrapRecord record{
        ConfigurationBootstrapSequence(envelope.versionValue),
        ConfigurationStorageFormatVersion(format), envelope.storageEpoch,
        static_cast<ConfigurationBootstrapState>(state)};
    if (!isPlausible(record)) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }
    return {ConfigurationBootstrapCodecStatus::Success, record};
}

}  // namespace fermentation
