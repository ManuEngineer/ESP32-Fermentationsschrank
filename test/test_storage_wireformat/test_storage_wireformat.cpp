#include <unity.h>

#include <cstdint>
#include <limits>
#include <string>

#include "big_endian_codec.hpp"
#include "binary64_codec.hpp"
#include "byte_buffer.hpp"
#include "crc32.hpp"
#include "storage_envelope.hpp"
#include "storage_types.hpp"

namespace {

using device_platform::ByteReader;
using device_platform::ByteWriter;
namespace be = device_platform::big_endian;
namespace bin64 = device_platform::binary64;

std::string bytesOf(std::initializer_list<uint8_t> values) {
    return std::string(values.begin(), values.end());
}

void test_byte_writer_rejects_writes_beyond_capacity() {
    ByteWriter writer(3U);
    TEST_ASSERT_TRUE(writer.writeByte(0x01U));
    TEST_ASSERT_TRUE(writer.writeByte(0x02U));
    TEST_ASSERT_EQUAL_UINT32(1U, writer.remaining());
    const uint8_t twoBytes[2] = {0x03U, 0x04U};
    TEST_ASSERT_FALSE(writer.writeBytes(twoBytes, 2U));
    TEST_ASSERT_EQUAL_UINT32(2U, writer.size());
    TEST_ASSERT_TRUE(writer.writeByte(0x03U));
    TEST_ASSERT_EQUAL_UINT32(0U, writer.remaining());
    TEST_ASSERT_FALSE(writer.writeByte(0x04U));
}

void test_byte_reader_rejects_reads_beyond_available_bytes() {
    const std::string data = bytesOf({0x01U, 0x02U});
    ByteReader reader(data);
    uint8_t value = 0;
    TEST_ASSERT_TRUE(reader.readByte(value));
    TEST_ASSERT_EQUAL_UINT8(0x01U, value);
    uint8_t twoBytes[2] = {0, 0};
    TEST_ASSERT_FALSE(reader.readBytes(twoBytes, 2U));
    TEST_ASSERT_EQUAL_UINT32(1U, reader.position());
    TEST_ASSERT_TRUE(reader.readByte(value));
    TEST_ASSERT_EQUAL_UINT8(0x02U, value);
    TEST_ASSERT_EQUAL_UINT32(0U, reader.remaining());
}

void test_big_endian_unsigned_golden_bytes() {
    ByteWriter writer(2U + 4U + 8U);
    TEST_ASSERT_TRUE(be::writeUint16(writer, 0x0102U));
    TEST_ASSERT_TRUE(be::writeUint32(writer, 0x01020304U));
    TEST_ASSERT_TRUE(be::writeUint64(writer, 0x0102030405060708ULL));
    const std::string expected = bytesOf({
        0x01U, 0x02U,                                            // uint16
        0x01U, 0x02U, 0x03U, 0x04U,                              // uint32
        0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U,  // uint64
    });
    TEST_ASSERT_EQUAL_UINT32(expected.size(), writer.size());
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), writer.bytes().data(),
                             expected.size());

    ByteReader reader(writer.bytes());
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;
    TEST_ASSERT_TRUE(be::readUint16(reader, u16));
    TEST_ASSERT_TRUE(be::readUint32(reader, u32));
    TEST_ASSERT_TRUE(be::readUint64(reader, u64));
    TEST_ASSERT_EQUAL_UINT16(0x0102U, u16);
    TEST_ASSERT_EQUAL_UINT32(0x01020304U, u32);
    TEST_ASSERT_EQUAL_UINT64(0x0102030405060708ULL, u64);
}

void test_big_endian_signed_two_complement_golden_bytes() {
    ByteWriter writer(1U + 2U + 4U + 8U);
    TEST_ASSERT_TRUE(be::writeInt8(writer, -1));
    TEST_ASSERT_TRUE(be::writeInt16(writer, -2));
    TEST_ASSERT_TRUE(be::writeInt32(writer, -3));
    TEST_ASSERT_TRUE(be::writeInt64(writer, -4));
    const std::string expected = bytesOf({
        0xFFU,                                                   // -1 as int8
        0xFFU, 0xFEU,                                            // -2 as int16
        0xFFU, 0xFFU, 0xFFU, 0xFDU,                              // -3 as int32
        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFCU,  // -4 as int64
    });
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), writer.bytes().data(),
                             expected.size());

    ByteReader reader(writer.bytes());
    int8_t i8 = 0;
    int16_t i16 = 0;
    int32_t i32 = 0;
    int64_t i64 = 0;
    TEST_ASSERT_TRUE(be::readInt8(reader, i8));
    TEST_ASSERT_TRUE(be::readInt16(reader, i16));
    TEST_ASSERT_TRUE(be::readInt32(reader, i32));
    TEST_ASSERT_TRUE(be::readInt64(reader, i64));
    TEST_ASSERT_EQUAL_INT8(-1, i8);
    TEST_ASSERT_EQUAL_INT16(-2, i16);
    TEST_ASSERT_EQUAL_INT32(-3, i32);
    TEST_ASSERT_EQUAL_INT64(-4, i64);
}

void test_bool_and_optional_tag_accept_only_zero_and_one() {
    ByteWriter writer(2U);
    TEST_ASSERT_TRUE(be::writeBool(writer, true));
    TEST_ASSERT_TRUE(be::writeOptionalTag(writer, false));
    const std::string expected = bytesOf({0x01U, 0x00U});
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), writer.bytes().data(), 2U);

    ByteReader reader(writer.bytes());
    bool boolValue = false;
    bool tagValue = true;
    TEST_ASSERT_TRUE(be::readBool(reader, boolValue));
    TEST_ASSERT_TRUE(be::readOptionalTag(reader, tagValue));
    TEST_ASSERT_TRUE(boolValue);
    TEST_ASSERT_FALSE(tagValue);

    const std::string invalid = bytesOf({0x02U});
    ByteReader invalidReader(invalid);
    bool ignored = false;
    TEST_ASSERT_FALSE(be::readBool(invalidReader, ignored));
}

void test_binary64_golden_values_round_trip() {
    // 1.5 = 0x3FF8000000000000
    ByteWriter writer(8U);
    TEST_ASSERT_TRUE(bin64::encode(1.5, writer));
    const std::string expected =
        bytesOf({0x3FU, 0xF8U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), writer.bytes().data(), 8U);

    ByteReader reader(writer.bytes());
    double decoded = 0.0;
    TEST_ASSERT_TRUE(bin64::decode(reader, decoded));
    TEST_ASSERT_EQUAL_DOUBLE(1.5, decoded);
}

void test_binary64_negative_zero_is_normalized_on_encode() {
    ByteWriter writer(8U);
    TEST_ASSERT_TRUE(bin64::encode(-0.0, writer));
    const std::string positiveZero =
        bytesOf({0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    TEST_ASSERT_EQUAL_MEMORY(positiveZero.data(), writer.bytes().data(), 8U);
}

void test_binary64_decode_rejects_noncanonical_negative_zero() {
    const std::string negativeZeroBits =
        bytesOf({0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    ByteReader reader(negativeZeroBits);
    double decoded = 0.0;
    TEST_ASSERT_FALSE(bin64::decode(reader, decoded));
}

void test_binary64_rejects_nan_and_infinity_both_ways() {
    ByteWriter writer(8U);
    TEST_ASSERT_FALSE(
        bin64::encode(std::numeric_limits<double>::quiet_NaN(), writer));
    TEST_ASSERT_FALSE(
        bin64::encode(std::numeric_limits<double>::infinity(), writer));
    TEST_ASSERT_FALSE(
        bin64::encode(-std::numeric_limits<double>::infinity(), writer));
    TEST_ASSERT_EQUAL_UINT32(0U, writer.size());

    // Golden NaN bit pattern (quiet NaN, 0x7FF8000000000000).
    const std::string nanBits =
        bytesOf({0x7FU, 0xF8U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    ByteReader nanReader(nanBits);
    double decoded = 0.0;
    TEST_ASSERT_FALSE(bin64::decode(nanReader, decoded));

    // +Infinity bit pattern (0x7FF0000000000000).
    const std::string infBits =
        bytesOf({0x7FU, 0xF0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    ByteReader infReader(infBits);
    TEST_ASSERT_FALSE(bin64::decode(infReader, decoded));

    // -Infinity bit pattern (0xFFF0000000000000).
    const std::string negInfBits =
        bytesOf({0xFFU, 0xF0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    ByteReader negInfReader(negInfBits);
    TEST_ASSERT_FALSE(bin64::decode(negInfReader, decoded));
}

device_platform::StorageEnvelope validEnvelope() {
    device_platform::StorageEnvelope envelope;
    envelope.recordTypeId = device_platform::RecordTypeId(7U);
    envelope.schemaVersion = 3U;
    envelope.storageEpoch = device_platform::StorageEpoch(1U);
    envelope.versionValue = 42U;
    envelope.changeOriginWireId = 2U;
    envelope.changeOperationWireId = 5U;
    envelope.payload = "AB";
    return envelope;
}

// Golden-Bytes unabhaengig mit Python `zlib.crc32` (identischer
// CRC-32/ISO-HDLC-Algorithmus) berechnet, nicht durch Aufruf der
// Produktionsfunktion erzeugt.
void test_envelope_golden_vector_without_utc() {
    const auto envelope = validEnvelope();
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
        static_cast<int>(
            device_platform::encodeEnvelope(envelope, encoded, 1024U)));

    const std::string expected = bytesOf({
        0x44U, 0x50U, 0x52U, 0x46U,  // Magic "DPRF"
        0x00U, 0x01U,                // Envelope-Version 1
        0x00U, 0x07U,                // RecordTypeId 7
        0x00U, 0x00U, 0x00U, 0x03U,  // SchemaVersion 3
        0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x01U,  // StorageEpoch 1
        0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x2AU,  // VersionValue 42
        0x00U, 0x00U, 0x00U, 0x02U,  // Payloadlaenge 2
        0x00U, 0x02U,                // ChangeOrigin 2
        0x00U, 0x05U,                // ChangeOperation 5
        0x00U,                       // kein UTC
        0xF0U, 0xB6U, 0x62U, 0x19U,  // CRC-32/ISO-HDLC
        0x41U, 0x42U,                // Payload "AB"
    });
    TEST_ASSERT_EQUAL_UINT32(43U, expected.size());
    TEST_ASSERT_EQUAL_UINT32(expected.size(), encoded.size());
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), encoded.data(), expected.size());

    const auto decoded = device_platform::decodeEnvelope(expected);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::Success),
        static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.envelope.has_value());
    TEST_ASSERT_EQUAL_UINT16(7U, decoded.envelope->recordTypeId.value());
    TEST_ASSERT_EQUAL_UINT32(3U, decoded.envelope->schemaVersion);
    TEST_ASSERT_EQUAL_UINT64(1U, decoded.envelope->storageEpoch.value());
    TEST_ASSERT_EQUAL_UINT64(42U, decoded.envelope->versionValue);
    TEST_ASSERT_EQUAL_UINT16(2U, decoded.envelope->changeOriginWireId);
    TEST_ASSERT_EQUAL_UINT16(5U, decoded.envelope->changeOperationWireId);
    TEST_ASSERT_FALSE(decoded.envelope->utcUnixSeconds.has_value());
    TEST_ASSERT_EQUAL_STRING("AB", decoded.envelope->payload.c_str());
}

void test_envelope_golden_vector_with_utc() {
    auto envelope = validEnvelope();
    envelope.utcUnixSeconds = 1700000000;
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
        static_cast<int>(
            device_platform::encodeEnvelope(envelope, encoded, 1024U)));

    const std::string expected = bytesOf({
        0x44U, 0x50U, 0x52U, 0x46U, 0x00U, 0x01U, 0x00U, 0x07U, 0x00U,
        0x00U, 0x00U, 0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x2AU, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x02U, 0x00U, 0x05U,
        0x01U,  // UTC-Tag gesetzt
        0x00U, 0x00U, 0x00U, 0x00U, 0x65U, 0x53U, 0xF1U, 0x00U,  // 1700000000
        0xB6U, 0x2AU, 0x94U, 0x87U,  // CRC-32/ISO-HDLC
        0x41U, 0x42U,                // Payload "AB"
    });
    TEST_ASSERT_EQUAL_UINT32(51U, expected.size());
    TEST_ASSERT_EQUAL_UINT32(expected.size(), encoded.size());
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), encoded.data(), expected.size());

    const auto decoded = device_platform::decodeEnvelope(expected);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::Success),
        static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.envelope->utcUnixSeconds.has_value());
    TEST_ASSERT_EQUAL_INT64(1700000000, *decoded.envelope->utcUnixSeconds);
}

void test_envelope_encode_rejects_reserved_zero_fields() {
    std::string encoded;
    auto invalidRecordType = validEnvelope();
    invalidRecordType.recordTypeId = device_platform::RecordTypeId(0U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::InvalidField),
        static_cast<int>(device_platform::encodeEnvelope(invalidRecordType,
                                                         encoded, 1024U)));

    auto invalidSchema = validEnvelope();
    invalidSchema.schemaVersion = 0U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::InvalidField),
        static_cast<int>(
            device_platform::encodeEnvelope(invalidSchema, encoded, 1024U)));

    auto invalidEpoch = validEnvelope();
    invalidEpoch.storageEpoch = device_platform::StorageEpoch(0U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::InvalidField),
        static_cast<int>(
            device_platform::encodeEnvelope(invalidEpoch, encoded, 1024U)));

    auto invalidVersionValue = validEnvelope();
    invalidVersionValue.versionValue = 0U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::InvalidField),
        static_cast<int>(device_platform::encodeEnvelope(invalidVersionValue,
                                                         encoded, 1024U)));
}

void test_envelope_encode_rejects_capacity_overflow() {
    auto envelope = validEnvelope();
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeEncodeStatus::CapacityExceeded),
        static_cast<int>(
            device_platform::encodeEnvelope(envelope, encoded, 10U)));
    TEST_ASSERT_TRUE(encoded.empty());
}

void test_envelope_decode_rejects_invalid_magic() {
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(validEnvelope(), encoded, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    encoded[0] = 'X';
    const auto decoded = device_platform::decodeEnvelope(encoded);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::InvalidMagic),
        static_cast<int>(decoded.status));
    TEST_ASSERT_FALSE(decoded.envelope.has_value());
}

void test_envelope_decode_rejects_unknown_envelope_version() {
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(validEnvelope(), encoded, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    encoded[5] = 0x02;  // Envelope-Version-Low-Byte auf 2 setzen
    const auto decoded = device_platform::decodeEnvelope(encoded);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeDecodeStatus::UnknownEnvelopeVersion),
        static_cast<int>(decoded.status));
}

void test_envelope_decode_rejects_reserved_zero_fields() {
    // RecordTypeId (Byte 6-7), SchemaVersion (8-11), StorageEpoch (12-19),
    // VersionValue (20-27) je auf 0 setzen und prüfen.
    auto zeroOut = [](std::string bytes, std::size_t offset,
                      std::size_t length) {
        for (std::size_t i = 0U; i < length; ++i) {
            bytes[offset + i] = static_cast<char>(0);
        }
        return bytes;
    };
    std::string base;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(validEnvelope(), base, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeDecodeStatus::InvalidRecordType),
        static_cast<int>(
            device_platform::decodeEnvelope(zeroOut(base, 6U, 2U)).status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeDecodeStatus::InvalidSchemaVersion),
        static_cast<int>(
            device_platform::decodeEnvelope(zeroOut(base, 8U, 4U)).status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeDecodeStatus::InvalidStorageEpoch),
        static_cast<int>(
            device_platform::decodeEnvelope(zeroOut(base, 12U, 8U)).status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeDecodeStatus::InvalidVersionValue),
        static_cast<int>(
            device_platform::decodeEnvelope(zeroOut(base, 20U, 8U)).status));
}

void test_envelope_decode_rejects_invalid_utc_tag() {
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(validEnvelope(), encoded, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    encoded[36] = static_cast<char>(0x02U);  // UTC-Tag-Byte
    const auto decoded = device_platform::decodeEnvelope(encoded);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::InvalidUtcTag),
        static_cast<int>(decoded.status));
}

void test_envelope_decode_rejects_wrong_and_overflowing_lengths() {
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(validEnvelope(), encoded, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);

    // Fehlende Bytes (Payload abgeschnitten).
    const auto truncated = encoded.substr(0U, encoded.size() - 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::LengthMismatch),
        static_cast<int>(device_platform::decodeEnvelope(truncated).status));

    // Zusaetzliche Bytes.
    const auto extended = encoded + std::string(1U, 'Z');
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::LengthMismatch),
        static_cast<int>(device_platform::decodeEnvelope(extended).status));

    // Ueberlaufende behauptete Laenge (0xFFFFFFFF), aber tatsaechlich nur
    // wenige Bytes vorhanden: darf keine Allokation in dieser Groesse
    // ausloesen, nur ablehnen.
    auto overflowing = encoded;
    overflowing[28] = static_cast<char>(0xFFU);
    overflowing[29] = static_cast<char>(0xFFU);
    overflowing[30] = static_cast<char>(0xFFU);
    overflowing[31] = static_cast<char>(0xFFU);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::LengthMismatch),
        static_cast<int>(device_platform::decodeEnvelope(overflowing).status));
}

void test_envelope_decode_rejects_crc_errors_in_header_and_payload() {
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(validEnvelope(), encoded, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);

    auto headerCorrupted = encoded;
    headerCorrupted[8] = static_cast<char>(headerCorrupted[8] ^ 0x01);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::CrcMismatch),
        static_cast<int>(
            device_platform::decodeEnvelope(headerCorrupted).status));

    auto payloadCorrupted = encoded;
    payloadCorrupted.back() = static_cast<char>(payloadCorrupted.back() ^ 0x01);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::CrcMismatch),
        static_cast<int>(
            device_platform::decodeEnvelope(payloadCorrupted).status));
}

void test_envelope_preserves_unknown_origin_and_operation_ids() {
    auto envelope = validEnvelope();
    envelope.changeOriginWireId = 0xBEEFU;
    envelope.changeOperationWireId = 0xF00DU;
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(envelope, encoded, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    const auto decoded = device_platform::decodeEnvelope(encoded);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeDecodeStatus::Success),
        static_cast<int>(decoded.status));
    TEST_ASSERT_EQUAL_UINT16(0xBEEFU, decoded.envelope->changeOriginWireId);
    TEST_ASSERT_EQUAL_UINT16(0xF00DU, decoded.envelope->changeOperationWireId);
}

void test_crc32_iso_hdlc_check_value() {
    // Verbindlicher Pruefwert aus docs/CONFIGURATION_PERSISTENCE.md,
    // Abschnitt "CRC-32/ISO-HDLC".
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926U, device_platform::computeCrc32IsoHdlc(
                                              std::string("123456789")));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_byte_writer_rejects_writes_beyond_capacity);
    RUN_TEST(test_byte_reader_rejects_reads_beyond_available_bytes);
    RUN_TEST(test_big_endian_unsigned_golden_bytes);
    RUN_TEST(test_big_endian_signed_two_complement_golden_bytes);
    RUN_TEST(test_bool_and_optional_tag_accept_only_zero_and_one);
    RUN_TEST(test_binary64_golden_values_round_trip);
    RUN_TEST(test_binary64_negative_zero_is_normalized_on_encode);
    RUN_TEST(test_binary64_decode_rejects_noncanonical_negative_zero);
    RUN_TEST(test_binary64_rejects_nan_and_infinity_both_ways);
    RUN_TEST(test_crc32_iso_hdlc_check_value);
    RUN_TEST(test_envelope_golden_vector_without_utc);
    RUN_TEST(test_envelope_golden_vector_with_utc);
    RUN_TEST(test_envelope_encode_rejects_reserved_zero_fields);
    RUN_TEST(test_envelope_encode_rejects_capacity_overflow);
    RUN_TEST(test_envelope_decode_rejects_invalid_magic);
    RUN_TEST(test_envelope_decode_rejects_unknown_envelope_version);
    RUN_TEST(test_envelope_decode_rejects_reserved_zero_fields);
    RUN_TEST(test_envelope_decode_rejects_invalid_utc_tag);
    RUN_TEST(test_envelope_decode_rejects_wrong_and_overflowing_lengths);
    RUN_TEST(test_envelope_decode_rejects_crc_errors_in_header_and_payload);
    RUN_TEST(test_envelope_preserves_unknown_origin_and_operation_ids);
    return UNITY_END();
}
