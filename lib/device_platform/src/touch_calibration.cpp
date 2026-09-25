#include "touch_calibration.hpp"

#include <cmath>
#include <utility>

#include "big_endian_codec.hpp"
#include "binary64_codec.hpp"
#include "byte_buffer.hpp"
#include "storage_envelope.hpp"

namespace device_platform {
namespace {

bool writeString(ByteWriter& writer, const std::string& value) {
    if (value.size() > 0xFFFFU) return false;
    return big_endian::writeUint16(writer,
                                   static_cast<std::uint16_t>(value.size())) &&
           writer.writeBytes(value.data(), value.size());
}

bool readString(ByteReader& reader, std::size_t maximumBytes,
                std::string& out) {
    std::uint16_t length = 0U;
    if (!big_endian::readUint16(reader, length) || length > maximumBytes ||
        length > reader.remaining()) {
        return false;
    }
    std::string value(length, '\0');
    if (!reader.readBytes(value.data(), length)) {
        return false;
    }
    out = std::move(value);
    return true;
}

StateStoreKey makeStateStoreKey(const char* bytes) {
    const auto result = StateStoreKey::create(bytes);
    // The key literal is a compile-time contract value validated by the
    // port; both "tc0" and "tc1" satisfy StateStoreKey's charset/length
    // rule.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return *result.key;
}

}  // namespace

CalibratedTouchPoint applyTouchCalibration(const TouchCalibrationModel& model,
                                           std::uint16_t rawX,
                                           std::uint16_t rawY) noexcept {
    const auto x = static_cast<double>(rawX);
    const auto y = static_cast<double>(rawY);
    return {model.a * x + model.b * y + model.c,
            model.d * x + model.e * y + model.f};
}

bool touchCalibrationModelIsWellFormed(
    const TouchCalibrationModel& model,
    const std::string& expectedBoardControllerId) noexcept {
    if (expectedBoardControllerId.empty()) return false;
    if (model.boardControllerId != expectedBoardControllerId) return false;
    return std::isfinite(model.a) && std::isfinite(model.b) &&
           std::isfinite(model.c) && std::isfinite(model.d) &&
           std::isfinite(model.e) && std::isfinite(model.f);
}

TouchCalibrationCodecStatus encodeTouchCalibrationPayload(
    const TouchCalibrationModel& model, std::string& out) {
    if (model.boardControllerId.empty() ||
        model.boardControllerId.size() >
            kMaximumTouchCalibrationBoardControllerIdBytes) {
        return TouchCalibrationCodecStatus::InvalidWireValue;
    }
    if (!std::isfinite(model.a) || !std::isfinite(model.b) ||
        !std::isfinite(model.c) || !std::isfinite(model.d) ||
        !std::isfinite(model.e) || !std::isfinite(model.f)) {
        return TouchCalibrationCodecStatus::InvalidWireValue;
    }
    ByteWriter writer(kMaximumTouchCalibrationPayloadBytes);
    if (!binary64::encode(model.a, writer) ||
        !binary64::encode(model.b, writer) ||
        !binary64::encode(model.c, writer) ||
        !binary64::encode(model.d, writer) ||
        !binary64::encode(model.e, writer) ||
        !binary64::encode(model.f, writer) ||
        !writeString(writer, model.boardControllerId)) {
        return TouchCalibrationCodecStatus::CapacityExceeded;
    }
    out = writer.takeBytes();
    return TouchCalibrationCodecStatus::Success;
}

TouchCalibrationPayloadDecodeResult decodeTouchCalibrationPayload(
    const std::string& payload) {
    if (payload.size() > kMaximumTouchCalibrationPayloadBytes) {
        return {TouchCalibrationCodecStatus::CapacityExceeded, std::nullopt};
    }
    ByteReader reader(payload);
    TouchCalibrationModel model;
    if (!binary64::decode(reader, model.a) ||
        !binary64::decode(reader, model.b) ||
        !binary64::decode(reader, model.c) ||
        !binary64::decode(reader, model.d) ||
        !binary64::decode(reader, model.e) ||
        !binary64::decode(reader, model.f) ||
        !readString(reader, kMaximumTouchCalibrationBoardControllerIdBytes,
                    model.boardControllerId)) {
        return {TouchCalibrationCodecStatus::Truncated, std::nullopt};
    }
    if (reader.remaining() != 0U) {
        return {TouchCalibrationCodecStatus::TrailingBytes, std::nullopt};
    }
    if (model.boardControllerId.empty()) {
        return {TouchCalibrationCodecStatus::InvalidWireValue, std::nullopt};
    }
    return {TouchCalibrationCodecStatus::Success, std::move(model)};
}

const StateStoreKey& TouchCalibrationStore::key(
    TouchCalibrationSlot slot) noexcept {
    static const auto active =
        makeStateStoreKey(kTouchCalibrationActiveKeyBytes);
    static const auto fallback =
        makeStateStoreKey(kTouchCalibrationFallbackKeyBytes);
    return slot == TouchCalibrationSlot::Active ? active : fallback;
}

TouchCalibrationLoadResult TouchCalibrationStore::load(
    TouchCalibrationSlot slot,
    const std::string& expectedBoardControllerId) const {
    const auto read =
        store_.read(key(slot), kMaximumTouchCalibrationEnvelopeBytes);
    if (read.status == StateStoreReadStatus::NotFound) {
        return {TouchCalibrationLoadStatus::NotFound, std::nullopt};
    }
    if (read.status == StateStoreReadStatus::CapacityError) {
        return {TouchCalibrationLoadStatus::CapacityError, std::nullopt};
    }
    if (read.status != StateStoreReadStatus::Success) {
        return {TouchCalibrationLoadStatus::ReadError, std::nullopt};
    }
    const auto decoded = decodeEnvelope(read.value);
    if (!decoded.envelope.has_value() ||
        decoded.envelope->recordTypeId != kTouchCalibrationRecordType ||
        decoded.envelope->versionValue == 0U) {
        return {TouchCalibrationLoadStatus::InvalidRecord, std::nullopt};
    }
    if (decoded.envelope->schemaVersion != kTouchCalibrationSchemaVersion) {
        return {TouchCalibrationLoadStatus::UnsupportedSchema, std::nullopt};
    }
    if (decoded.envelope->storageEpoch != kTouchCalibrationStorageEpoch) {
        return {TouchCalibrationLoadStatus::OtherEpoch, std::nullopt};
    }
    const auto payload =
        decodeTouchCalibrationPayload(decoded.envelope->payload);
    if (!payload.model.has_value()) {
        return {TouchCalibrationLoadStatus::InvalidRecord, std::nullopt};
    }
    if (!touchCalibrationModelIsWellFormed(*payload.model,
                                           expectedBoardControllerId)) {
        return {TouchCalibrationLoadStatus::InvalidRecord, std::nullopt};
    }
    return {TouchCalibrationLoadStatus::Available,
            TouchCalibrationRecord{std::move(*payload.model),
                                   decoded.envelope->versionValue}};
}

TouchCalibrationWriteResult TouchCalibrationStore::write(
    TouchCalibrationSlot slot, const TouchCalibrationModel& model,
    std::uint64_t recordSequence) {
    if (recordSequence == 0U) {
        return {TouchCalibrationWriteStatus::WriteFailure, 0U};
    }
    std::string payload;
    if (encodeTouchCalibrationPayload(model, payload) !=
        TouchCalibrationCodecStatus::Success) {
        return {TouchCalibrationWriteStatus::CapacityFailure, 0U};
    }
    std::string encoded;
    const auto envelopeStatus = encodeEnvelope(
        {kTouchCalibrationRecordType, kTouchCalibrationSchemaVersion,
         kTouchCalibrationStorageEpoch, recordSequence, std::nullopt, payload},
        encoded, kMaximumTouchCalibrationEnvelopeBytes);
    if (envelopeStatus == EnvelopeEncodeStatus::CapacityExceeded) {
        return {TouchCalibrationWriteStatus::CapacityFailure, 0U};
    }
    if (envelopeStatus != EnvelopeEncodeStatus::Success) {
        return {TouchCalibrationWriteStatus::WriteFailure, 0U};
    }
    const auto writeStatus = store_.write(key(slot), encoded);
    if (writeStatus == StateStoreWriteStatus::WriteError) {
        return {TouchCalibrationWriteStatus::WriteFailure, 0U};
    }
    if (writeStatus == StateStoreWriteStatus::CapacityError) {
        return {TouchCalibrationWriteStatus::CapacityFailure, 0U};
    }
    const auto read =
        store_.read(key(slot), kMaximumTouchCalibrationEnvelopeBytes);
    if (read.status == StateStoreReadStatus::Success && read.value == encoded) {
        return {TouchCalibrationWriteStatus::Committed, recordSequence};
    }
    // A successful write with a failed or non-matching readback is not a
    // known pre-write failure; the value may already be durable. See
    // ConnectivityCredentialStore::write() for the identical reasoning.
    return {TouchCalibrationWriteStatus::Indeterminate, 0U};
}

}  // namespace device_platform
