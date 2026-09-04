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
    // This is the sole production encoder.  Legacy schemas are decode-only;
    // callers must not be able to create a new schema-1 writer by selecting a
    // field in the in-memory model.
    if (record.schemaVersion != kConfigurationBootstrapSchemaVersion2 ||
        !isPlausible(record)) {
        return ConfigurationBootstrapCodecStatus::InvalidModel;
    }

    const auto payloadSize = record.handoff == RunEpochHandoffState::None
                                 ? configuration_limits::
                                       kConfigurationBootstrapPayloadBytes
                                 : configuration_limits::
                                       kConfigurationBootstrapBoundPayloadBytes;
    device_platform::ByteWriter payload(payloadSize);
    if (!device_platform::big_endian::writeUint32(
            payload, record.storageFormatVersion.value()) ||
        !device_platform::big_endian::writeUint8(
            payload, static_cast<std::uint8_t>(record.state)) ||
        !device_platform::big_endian::writeUint8(
            payload, static_cast<std::uint8_t>(record.handoff))) {
        return ConfigurationBootstrapCodecStatus::CapacityExceeded;
    }
    if (record.handoff != RunEpochHandoffState::None &&
        (!record.previousEpoch.has_value() || !record.currentEpoch.has_value() ||
         !device_platform::big_endian::writeUint64(
             payload, record.previousEpoch->value()) ||
         !device_platform::big_endian::writeUint64(
             payload, record.currentEpoch->value()))) {
        return ConfigurationBootstrapCodecStatus::CapacityExceeded;
    }
    if (payload.size() != payloadSize) {
        return ConfigurationBootstrapCodecStatus::InvalidModel;
    }

    const device_platform::StorageEnvelope envelope{
        configuration_storage_contract::kConfigurationBootstrapRecordType,
        kConfigurationBootstrapSchemaVersion2,
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
    if (envelope.schemaVersion != kConfigurationBootstrapSchemaVersion1 &&
        envelope.schemaVersion != kConfigurationBootstrapSchemaVersion2) {
        return {envelope.schemaVersion > kConfigurationBootstrapSchemaVersion2
                    ? ConfigurationBootstrapCodecStatus::UnsupportedNewerSchema
                    : ConfigurationBootstrapCodecStatus::InvalidModel,
                std::nullopt};
    }
    if (envelope.utcUnixSeconds.has_value()) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }
    const auto expectedPayloadSize =
        envelope.schemaVersion == kConfigurationBootstrapSchemaVersion1
            ? configuration_limits::
                  kConfigurationBootstrapSchema1PayloadBytes
            : configuration_limits::kConfigurationBootstrapPayloadBytes;
    if (envelope.payload.size() != expectedPayloadSize &&
        envelope.schemaVersion == kConfigurationBootstrapSchemaVersion1) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }
    if (envelope.schemaVersion == kConfigurationBootstrapSchemaVersion2 &&
        envelope.payload.size() !=
            configuration_limits::kConfigurationBootstrapPayloadBytes &&
        envelope.payload.size() !=
            configuration_limits::kConfigurationBootstrapBoundPayloadBytes) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }

    device_platform::ByteReader reader(envelope.payload);
    std::uint32_t format = 0U;
    std::uint8_t state = 0U;
    std::uint8_t handoff =
        static_cast<std::uint8_t>(RunEpochHandoffState::None);
    if (!device_platform::big_endian::readUint32(reader, format) ||
        !device_platform::big_endian::readUint8(reader, state) ||
        (envelope.schemaVersion == kConfigurationBootstrapSchemaVersion2 &&
         !device_platform::big_endian::readUint8(reader, handoff))) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }
    if (format == 0U) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }
    if (format > kConfigurationStorageFormatVersion1.value()) {
        return {ConfigurationBootstrapCodecStatus::UnsupportedNewerSchema,
                std::nullopt};
    }
    std::optional<device_platform::StorageEpoch> previousEpoch;
    std::optional<device_platform::StorageEpoch> currentEpoch;
    if (envelope.schemaVersion == kConfigurationBootstrapSchemaVersion2 &&
        handoff != static_cast<std::uint8_t>(RunEpochHandoffState::None)) {
        std::uint64_t previous = 0U;
        std::uint64_t current = 0U;
        if (!device_platform::big_endian::readUint64(reader, previous) ||
            !device_platform::big_endian::readUint64(reader, current)) {
            return {ConfigurationBootstrapCodecStatus::InvalidModel,
                    std::nullopt};
        }
        previousEpoch = device_platform::StorageEpoch{previous};
        currentEpoch = device_platform::StorageEpoch{current};
    }
    if (reader.remaining() != 0U) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }

    ConfigurationBootstrapRecord record{
        ConfigurationBootstrapSequence{envelope.versionValue},
        ConfigurationStorageFormatVersion{format}, envelope.storageEpoch,
        static_cast<ConfigurationBootstrapState>(state),
        envelope.schemaVersion,
        static_cast<RunEpochHandoffState>(handoff), previousEpoch, currentEpoch};
    if (!isPlausible(record)) {
        return {ConfigurationBootstrapCodecStatus::InvalidModel, std::nullopt};
    }
    return {ConfigurationBootstrapCodecStatus::Success, std::move(record)};
}

}  // namespace fermentation
