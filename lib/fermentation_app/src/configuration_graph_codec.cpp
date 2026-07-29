#include "configuration_graph_codec.hpp"

#include <utility>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

using device_platform::ByteReader;
using device_platform::ByteWriter;

template <typename Version>
bool writeReference(ByteWriter& writer,
                    const ConfigurationRecordReference<Version>& reference) {
    using namespace device_platform::big_endian;
    return writeUint16(writer, reference.recordType.value()) &&
           writeUint32(writer, reference.slot.value()) &&
           writeUint64(writer, reference.version.value()) &&
           writeUint32(writer, reference.schemaVersion) &&
           writeUint32(writer, reference.payloadLength) &&
           writeUint32(writer, reference.payloadCrc) &&
           writeUint64(writer, reference.storageEpoch.value());
}

template <typename Version>
bool readReference(ByteReader& reader,
                   ConfigurationRecordReference<Version>& reference) {
    using namespace device_platform::big_endian;
    std::uint16_t recordType = 0U;
    std::uint32_t slot = 0U;
    std::uint64_t version = 0U;
    std::uint64_t epoch = 0U;
    if (!readUint16(reader, recordType) || !readUint32(reader, slot) ||
        !readUint64(reader, version) ||
        !readUint32(reader, reference.schemaVersion) ||
        !readUint32(reader, reference.payloadLength) ||
        !readUint32(reader, reference.payloadCrc) ||
        !readUint64(reader, epoch)) {
        return false;
    }
    reference.recordType = device_platform::RecordTypeId(recordType);
    reference.slot = device_platform::SlotId(slot);
    reference.version = Version(version);
    reference.storageEpoch = device_platform::StorageEpoch(epoch);
    return true;
}

ConfigurationGraphCodecStatus mapEnvelopeStatus(
    device_platform::EnvelopeEncodeStatus status) {
    if (status == device_platform::EnvelopeEncodeStatus::CapacityExceeded) {
        return ConfigurationGraphCodecStatus::CapacityExceeded;
    }
    return status == device_platform::EnvelopeEncodeStatus::Success
               ? ConfigurationGraphCodecStatus::Success
               : ConfigurationGraphCodecStatus::InvalidModel;
}

template <typename Model, typename Version, typename Encoder>
ConfigurationGraphCodecStatus encodeRecord(
    const Model& model, Version version,
    device_platform::StorageEpoch storageEpoch,
    std::optional<std::int64_t> utcUnixSeconds,
    device_platform::RecordTypeId recordType, std::uint32_t schemaVersion,
    std::size_t maxEnvelopeBytes, Encoder encoder, std::string& out) {
    std::string payload;
    const auto payloadStatus = encoder(model, payload);
    if (payloadStatus != ConfigurationGraphCodecStatus::Success) {
        return payloadStatus;
    }
    const device_platform::StorageEnvelope envelope{
        recordType,      schemaVersion,  storageEpoch,
        version.value(), utcUnixSeconds, std::move(payload)};
    std::string encoded;
    const auto envelopeStatus =
        device_platform::encodeEnvelope(envelope, encoded, maxEnvelopeBytes);
    if (envelopeStatus == device_platform::EnvelopeEncodeStatus::Success) {
        out.swap(encoded);
    }
    return mapEnvelopeStatus(envelopeStatus);
}

}  // namespace

ConfigurationGraphCodecStatus encodeConfigurationManifestPayload(
    const ConfigurationManifest& manifest, std::string& out) {
    if (!isPlausible(manifest)) {
        return ConfigurationGraphCodecStatus::InvalidModel;
    }
    ByteWriter writer(configuration_limits::kConfigurationManifestPayloadBytes);
    if (!device_platform::big_endian::writeUint8(writer,
                                                 manifest.origin.wireValue) ||
        !device_platform::big_endian::writeUint8(
            writer, manifest.operation.wireValue) ||
        !writeReference(writer, manifest.userConfiguration) ||
        !writeReference(writer, manifest.serviceConfiguration) ||
        !writeReference(writer, manifest.programCatalog) ||
        writer.size() !=
            configuration_limits::kConfigurationManifestPayloadBytes) {
        return ConfigurationGraphCodecStatus::CapacityExceeded;
    }
    auto encoded = writer.takeBytes();
    out.swap(encoded);
    return ConfigurationGraphCodecStatus::Success;
}

ConfigurationGraphDecodeResult<ConfigurationManifest>
decodeConfigurationManifestPayload(const std::string& payload) {
    if (payload.size() <
        configuration_limits::kConfigurationManifestPayloadBytes) {
        return {ConfigurationGraphCodecStatus::Truncated, std::nullopt};
    }
    if (payload.size() >
        configuration_limits::kConfigurationManifestPayloadBytes) {
        return {ConfigurationGraphCodecStatus::TrailingBytes, std::nullopt};
    }
    ByteReader reader(payload);
    std::uint8_t origin = 0U;
    std::uint8_t operation = 0U;
    ConfigurationManifest manifest;
    if (!device_platform::big_endian::readUint8(reader, origin) ||
        !device_platform::big_endian::readUint8(reader, operation) ||
        !readReference(reader, manifest.userConfiguration) ||
        !readReference(reader, manifest.serviceConfiguration) ||
        !readReference(reader, manifest.programCatalog)) {
        return {ConfigurationGraphCodecStatus::Truncated, std::nullopt};
    }
    manifest.origin = decodeChangeOrigin(origin);
    manifest.operation = decodeChangeOperation(operation);
    if (!isPlausible(manifest)) {
        return {ConfigurationGraphCodecStatus::InvalidModel, std::nullopt};
    }
    return {ConfigurationGraphCodecStatus::Success, manifest};
}

ConfigurationGraphCodecStatus encodeConfigurationRootPayload(
    const ConfigurationRootRecord& root, std::string& out) {
    if (!isPlausible(root)) {
        return ConfigurationGraphCodecStatus::InvalidModel;
    }
    const std::size_t expected =
        root.fallback.has_value()
            ? configuration_limits::kConfigurationRootPayloadWithFallbackBytes
            : configuration_limits::
                  kConfigurationRootPayloadWithoutFallbackBytes;
    ByteWriter writer(expected);
    if (!writeReference(writer, root.active) ||
        !device_platform::big_endian::writeOptionalTag(
            writer, root.fallback.has_value()) ||
        (root.fallback.has_value() &&
         !writeReference(writer, *root.fallback)) ||
        writer.size() != expected) {
        return ConfigurationGraphCodecStatus::CapacityExceeded;
    }
    auto encoded = writer.takeBytes();
    out.swap(encoded);
    return ConfigurationGraphCodecStatus::Success;
}

ConfigurationGraphDecodeResult<ConfigurationRootRecord>
decodeConfigurationRootPayload(const std::string& payload) {
    if (payload.size() <
        configuration_limits::kConfigurationRootPayloadWithoutFallbackBytes) {
        return {ConfigurationGraphCodecStatus::Truncated, std::nullopt};
    }
    ByteReader reader(payload);
    ConfigurationRootRecord root;
    if (!readReference(reader, root.active)) {
        return {ConfigurationGraphCodecStatus::Truncated, std::nullopt};
    }
    std::uint8_t tag = 0U;
    if (!device_platform::big_endian::readUint8(reader, tag)) {
        return {ConfigurationGraphCodecStatus::Truncated, std::nullopt};
    }
    if (tag > 1U) {
        return {ConfigurationGraphCodecStatus::InvalidOptionalTag,
                std::nullopt};
    }
    if (tag == 1U) {
        ConfigurationManifestReference fallback;
        if (!readReference(reader, fallback)) {
            return {ConfigurationGraphCodecStatus::Truncated, std::nullopt};
        }
        root.fallback = fallback;
    }
    if (reader.remaining() != 0U) {
        return {ConfigurationGraphCodecStatus::TrailingBytes, std::nullopt};
    }
    if (!isPlausible(root)) {
        return {ConfigurationGraphCodecStatus::InvalidModel, std::nullopt};
    }
    return {ConfigurationGraphCodecStatus::Success, root};
}

ConfigurationGraphCodecStatus encodeConfigurationManifestRecord(
    const ConfigurationManifest& manifest,
    ConfigurationManifestGeneration generation,
    device_platform::StorageEpoch storageEpoch,
    std::optional<std::int64_t> utcUnixSeconds, std::string& out) {
    return encodeRecord(
        manifest, generation, storageEpoch, utcUnixSeconds,
        configuration_storage_contract::kConfigurationManifestRecordType,
        kConfigurationManifestSchemaVersion1,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes,
        encodeConfigurationManifestPayload, out);
}

ConfigurationGraphCodecStatus encodeConfigurationRootRecord(
    const ConfigurationRootRecord& root, ConfigurationRootSequence sequence,
    device_platform::StorageEpoch storageEpoch,
    std::optional<std::int64_t> utcUnixSeconds, std::string& out) {
    return encodeRecord(
        root, sequence, storageEpoch, utcUnixSeconds,
        configuration_storage_contract::kConfigurationRootRecordType,
        kConfigurationRootSchemaVersion1,
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes,
        encodeConfigurationRootPayload, out);
}

}  // namespace fermentation
