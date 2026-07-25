#include <unity.h>

#include <cstdint>
#include <limits>
#include <string>

#include "big_endian_codec.hpp"
#include "binary64_codec.hpp"
#include "byte_buffer.hpp"
#include "checked_size.hpp"
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

void test_byte_writer_zero_length_write_with_null_pointer_is_safe() {
    ByteWriter writer(2U);
    TEST_ASSERT_TRUE(writer.writeBytes(nullptr, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, writer.size());
    TEST_ASSERT_TRUE(writer.writeByte(0x01U));
    TEST_ASSERT_EQUAL_UINT32(1U, writer.size());
}

void test_byte_reader_zero_length_read_with_null_pointer_is_safe() {
    const std::string data = bytesOf({0x01U});
    ByteReader reader(data);
    TEST_ASSERT_TRUE(reader.readBytes(nullptr, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, reader.position());
    uint8_t value = 0;
    TEST_ASSERT_TRUE(reader.readByte(value));
    TEST_ASSERT_EQUAL_UINT8(0x01U, value);
}

// `nullptr` mit positiver Laenge wird beobachtbar abgelehnt statt
// dereferenziert; der Puffer bleibt vollstaendig unveraendert.
void test_byte_writer_rejects_null_pointer_with_positive_length() {
    ByteWriter writer(4U);
    TEST_ASSERT_TRUE(writer.writeByte(0x01U));
    TEST_ASSERT_FALSE(writer.writeBytes(nullptr, 1U));
    TEST_ASSERT_EQUAL_UINT32(1U, writer.size());
    TEST_ASSERT_EQUAL_UINT32(3U, writer.remaining());
    TEST_ASSERT_EQUAL_UINT8(0x01U, static_cast<uint8_t>(writer.bytes()[0]));
    TEST_ASSERT_TRUE(writer.writeByte(0x02U));
    TEST_ASSERT_EQUAL_UINT32(2U, writer.size());
}

// `nullptr` mit positiver Laenge wird beobachtbar abgelehnt statt
// dereferenziert; die Leseposition bleibt unveraendert.
void test_byte_reader_rejects_null_pointer_with_positive_length() {
    const std::string data = bytesOf({0x01U, 0x02U});
    ByteReader reader(data);
    uint8_t value = 0;
    TEST_ASSERT_TRUE(reader.readByte(value));
    TEST_ASSERT_EQUAL_UINT8(0x01U, value);
    TEST_ASSERT_FALSE(reader.readBytes(nullptr, 1U));
    TEST_ASSERT_EQUAL_UINT32(1U, reader.position());
    TEST_ASSERT_EQUAL_UINT32(1U, reader.remaining());
    TEST_ASSERT_TRUE(reader.readByte(value));
    TEST_ASSERT_EQUAL_UINT8(0x02U, value);
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

// Fester Golden-Test fuer `writeUint8`/`readUint8` - der bisherige unsigned
// Golden-Test begann erst bei `uint16`, obwohl die Produktions-API
// `writeUint8()`/`readUint8()` enthaelt.
void test_big_endian_uint8_golden_values() {
    const struct {
        uint8_t value;
        uint8_t byte;
    } cases[] = {
        {0x00U, 0x00U}, {0x01U, 0x01U}, {0x7FU, 0x7FU},
        {0x80U, 0x80U}, {0xFFU, 0xFFU},
    };
    for (const auto& testCase : cases) {
        ByteWriter writer(1U);
        TEST_ASSERT_TRUE(be::writeUint8(writer, testCase.value));
        TEST_ASSERT_EQUAL_UINT32(1U, writer.size());
        TEST_ASSERT_EQUAL_UINT8(testCase.byte,
                                static_cast<uint8_t>(writer.bytes()[0]));

        ByteReader reader(writer.bytes());
        uint8_t decoded = 0;
        TEST_ASSERT_TRUE(be::readUint8(reader, decoded));
        TEST_ASSERT_EQUAL_UINT8(testCase.value, decoded);
    }
}

// Lesen aus leerem Puffer schlaegt fehl und laesst Ausgabeparameter sowie
// Reader-Position unveraendert.
void test_big_endian_uint8_read_from_empty_buffer_fails_and_leaves_output_unchanged() {
    const std::string empty;
    ByteReader reader(empty);
    uint8_t value = 42U;
    TEST_ASSERT_FALSE(be::readUint8(reader, value));
    TEST_ASSERT_EQUAL_UINT8(42U, value);
    TEST_ASSERT_EQUAL_UINT32(0U, reader.position());
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

// Feste Golden-Bytes je Breite fuer 0, 1, INT_MAX, -1 und INT_MIN. Prueft
// die portable Zweierkomplement-Rekonstruktion unabhaengig von Encoder und
// Decoder gegeneinander (Werte und Bytes sind von Hand abgeleitet, siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Kanonisches Wireformat").
void test_big_endian_int8_golden_values() {
    const struct {
        int8_t value;
        uint8_t byte;
    } cases[] = {
        {0, 0x00U},
        {1, 0x01U},
        {std::numeric_limits<int8_t>::max(), 0x7FU},
        {-1, 0xFFU},
        {std::numeric_limits<int8_t>::min(), 0x80U},
    };
    for (const auto& testCase : cases) {
        ByteWriter writer(1U);
        TEST_ASSERT_TRUE(be::writeInt8(writer, testCase.value));
        TEST_ASSERT_EQUAL_UINT8(testCase.byte,
                                static_cast<uint8_t>(writer.bytes()[0]));

        ByteReader reader(writer.bytes());
        int8_t decoded = 0;
        TEST_ASSERT_TRUE(be::readInt8(reader, decoded));
        TEST_ASSERT_EQUAL_INT8(testCase.value, decoded);
    }
}

void test_big_endian_int16_golden_values() {
    const struct {
        int16_t value;
        std::initializer_list<uint8_t> bytes;
    } cases[] = {
        {0, {0x00U, 0x00U}},
        {1, {0x00U, 0x01U}},
        {std::numeric_limits<int16_t>::max(), {0x7FU, 0xFFU}},
        {-1, {0xFFU, 0xFFU}},
        {std::numeric_limits<int16_t>::min(), {0x80U, 0x00U}},
    };
    for (const auto& testCase : cases) {
        ByteWriter writer(2U);
        TEST_ASSERT_TRUE(be::writeInt16(writer, testCase.value));
        const std::string expected = bytesOf(testCase.bytes);
        TEST_ASSERT_EQUAL_MEMORY(expected.data(), writer.bytes().data(), 2U);

        ByteReader reader(writer.bytes());
        int16_t decoded = 0;
        TEST_ASSERT_TRUE(be::readInt16(reader, decoded));
        TEST_ASSERT_EQUAL_INT16(testCase.value, decoded);
    }
}

void test_big_endian_int32_golden_values() {
    const struct {
        int32_t value;
        std::initializer_list<uint8_t> bytes;
    } cases[] = {
        {0, {0x00U, 0x00U, 0x00U, 0x00U}},
        {1, {0x00U, 0x00U, 0x00U, 0x01U}},
        {std::numeric_limits<int32_t>::max(), {0x7FU, 0xFFU, 0xFFU, 0xFFU}},
        {-1, {0xFFU, 0xFFU, 0xFFU, 0xFFU}},
        {std::numeric_limits<int32_t>::min(), {0x80U, 0x00U, 0x00U, 0x00U}},
    };
    for (const auto& testCase : cases) {
        ByteWriter writer(4U);
        TEST_ASSERT_TRUE(be::writeInt32(writer, testCase.value));
        const std::string expected = bytesOf(testCase.bytes);
        TEST_ASSERT_EQUAL_MEMORY(expected.data(), writer.bytes().data(), 4U);

        ByteReader reader(writer.bytes());
        int32_t decoded = 0;
        TEST_ASSERT_TRUE(be::readInt32(reader, decoded));
        TEST_ASSERT_EQUAL_INT32(testCase.value, decoded);
    }
}

void test_big_endian_int64_golden_values() {
    const struct {
        int64_t value;
        std::initializer_list<uint8_t> bytes;
    } cases[] = {
        {0, {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}},
        {1, {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U}},
        {std::numeric_limits<int64_t>::max(),
         {0x7FU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU}},
        {-1, {0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU}},
        {std::numeric_limits<int64_t>::min(),
         {0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}},
    };
    for (const auto& testCase : cases) {
        ByteWriter writer(8U);
        TEST_ASSERT_TRUE(be::writeInt64(writer, testCase.value));
        const std::string expected = bytesOf(testCase.bytes);
        TEST_ASSERT_EQUAL_MEMORY(expected.data(), writer.bytes().data(), 8U);

        ByteReader reader(writer.bytes());
        int64_t decoded = 0;
        TEST_ASSERT_TRUE(be::readInt64(reader, decoded));
        TEST_ASSERT_EQUAL_INT64(testCase.value, decoded);
    }
}

void test_big_endian_signed_read_failure_leaves_output_unchanged() {
    const std::string empty;
    ByteReader reader(empty);

    int8_t i8 = 42;
    TEST_ASSERT_FALSE(be::readInt8(reader, i8));
    TEST_ASSERT_EQUAL_INT8(42, i8);

    int16_t i16 = 42;
    TEST_ASSERT_FALSE(be::readInt16(reader, i16));
    TEST_ASSERT_EQUAL_INT16(42, i16);

    int32_t i32 = 42;
    TEST_ASSERT_FALSE(be::readInt32(reader, i32));
    TEST_ASSERT_EQUAL_INT32(42, i32);

    int64_t i64 = 42;
    TEST_ASSERT_FALSE(be::readInt64(reader, i64));
    TEST_ASSERT_EQUAL_INT64(42, i64);
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

void test_binary64_decode_failure_leaves_output_unchanged() {
    const double sentinel = 7.0;

    const std::string nanBits =
        bytesOf({0x7FU, 0xF8U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    ByteReader nanReader(nanBits);
    double outNan = sentinel;
    TEST_ASSERT_FALSE(bin64::decode(nanReader, outNan));
    TEST_ASSERT_EQUAL_DOUBLE(sentinel, outNan);

    const std::string infBits =
        bytesOf({0x7FU, 0xF0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    ByteReader infReader(infBits);
    double outInf = sentinel;
    TEST_ASSERT_FALSE(bin64::decode(infReader, outInf));
    TEST_ASSERT_EQUAL_DOUBLE(sentinel, outInf);

    const std::string negativeZeroBits =
        bytesOf({0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U});
    ByteReader negativeZeroReader(negativeZeroBits);
    double outNegZero = sentinel;
    TEST_ASSERT_FALSE(bin64::decode(negativeZeroReader, outNegZero));
    TEST_ASSERT_EQUAL_DOUBLE(sentinel, outNegZero);

    // Zu wenige Bytes.
    const std::string tooShort = bytesOf({0x00U, 0x00U});
    ByteReader tooShortReader(tooShort);
    double outTooShort = sentinel;
    TEST_ASSERT_FALSE(bin64::decode(tooShortReader, outTooShort));
    TEST_ASSERT_EQUAL_DOUBLE(sentinel, outTooShort);
}

void test_checked_add_size_accepts_within_bound() {
    std::size_t out = 0U;
    TEST_ASSERT_TRUE(device_platform::checkedAddSize(2U, 3U, 10U, out));
    TEST_ASSERT_EQUAL_UINT32(5U, out);
}

void test_checked_add_size_accepts_exact_bound() {
    std::size_t out = 0U;
    TEST_ASSERT_TRUE(device_platform::checkedAddSize(5U, 5U, 10U, out));
    TEST_ASSERT_EQUAL_UINT32(10U, out);
}

void test_checked_add_size_rejects_one_over_bound() {
    std::size_t out = 123U;
    TEST_ASSERT_FALSE(device_platform::checkedAddSize(5U, 6U, 10U, out));
    TEST_ASSERT_EQUAL_UINT32(123U, out);
}

void test_checked_add_size_rejects_actual_size_t_wraparound() {
    std::size_t out = 123U;
    const std::size_t kSizeMax = std::numeric_limits<std::size_t>::max();
    TEST_ASSERT_FALSE(
        device_platform::checkedAddSize(kSizeMax, 1U, kSizeMax, out));
    TEST_ASSERT_EQUAL_UINT32(123U, out);
}

// Beweist die 32-Bit-Grenzsemantik ueber eine explizite `maxAllowed`, nicht
// ueber die native (auf dem Testhost 64-Bit-)Breite von `size_t`.
void test_checked_add_size_simulates_32_bit_boundary() {
    constexpr std::size_t kSimulated32BitMax = 0xFFFFFFFFULL;
    std::size_t out = 0U;
    TEST_ASSERT_TRUE(device_platform::checkedAddSize(0xFFFFFFFEULL, 1U,
                                                     kSimulated32BitMax, out));
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFULL, out);

    std::size_t rejectedOut = 123U;
    TEST_ASSERT_FALSE(device_platform::checkedAddSize(
        0xFFFFFFFFULL, 1U, kSimulated32BitMax, rejectedOut));
    TEST_ASSERT_EQUAL_UINT32(123U, rejectedOut);
}

// `checkEnvelopeEncodedSize()` prueft die Envelope-Groessenentscheidung ohne
// eine reale Payload dieser Groesse anzulegen - `payloadSize` ist nur ein
// Zahlenwert. `maxTotalBytes` wird bewusst auf `SIZE_MAX` gesetzt, damit
// ausschliesslich das 32-Bit-Wire-Laengenfeld die Ablehnung verursacht, nicht
// `maxTotalBytes`.
void test_check_envelope_encoded_size_accepts_payload_exactly_at_uint32_max() {
    const std::size_t exactUint32Max =
        static_cast<std::size_t>(std::numeric_limits<uint32_t>::max());
    const auto result = device_platform::checkEnvelopeEncodedSize(
        exactUint32Max, false, std::numeric_limits<std::size_t>::max());
    TEST_ASSERT_TRUE(result.status ==
                     device_platform::EnvelopeEncodeStatus::Success);
}

// Payloadgroesse eines Bytes ueber dem 32-Bit-Wire-Laengenfeld wird
// konsistent als `CapacityExceeded` abgelehnt, nicht als `InvalidField` -
// der Inhalt ist nicht fachlich ungueltig, sondern uebersteigt die
// technische Wireformat-Kapazitaet. Nur auf einem Testhost mit `size_t`
// breiter als 32 Bit direkt darstellbar (hier: nativer 64-Bit-Testhost).
void test_check_envelope_encoded_size_rejects_payload_one_byte_over_uint32_max_as_capacity_exceeded() {
    const std::size_t oneOverUint32Max =
        static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()) + 1U;
    const auto result = device_platform::checkEnvelopeEncodedSize(
        oneOverUint32Max, false, std::numeric_limits<std::size_t>::max());
    TEST_ASSERT_TRUE(result.status ==
                     device_platform::EnvelopeEncodeStatus::CapacityExceeded);
    TEST_ASSERT_EQUAL_UINT32(0U, result.totalSize);
}

// `encodeEnvelope()` nutzt `checkEnvelopeEncodedSize()` intern als einzige
// Quelle der Wahrheit - die exakte Grenze, ein Byte darueber sowie
// Header+CRC- und Header+Payload-Ueberlauf sind bereits unten durch
// `test_envelope_encode_accepts_exact_max_total_bytes`,
// `test_envelope_encode_rejects_one_byte_over_exact_size`,
// `test_envelope_encode_rejects_overflow_already_at_header_plus_crc` und
// `test_envelope_encode_rejects_overflow_at_header_plus_payload` ueber den
// oeffentlichen Encoder abgedeckt.

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

// Header(37) + CRC(4) = 41 Bytes ohne UTC; `validEnvelope()`-Payload "AB"
// ergibt eine Gesamtgroesse von exakt 43 Bytes (siehe Golden-Vektor-Test).
void test_envelope_encode_accepts_exact_max_total_bytes() {
    const auto envelope = validEnvelope();
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
        static_cast<int>(
            device_platform::encodeEnvelope(envelope, encoded, 43U)));
    TEST_ASSERT_EQUAL_UINT32(43U, encoded.size());
}

void test_envelope_encode_rejects_one_byte_over_exact_size() {
    const auto envelope = validEnvelope();
    std::string encoded = "UNCHANGED";
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeEncodeStatus::CapacityExceeded),
        static_cast<int>(
            device_platform::encodeEnvelope(envelope, encoded, 42U)));
    TEST_ASSERT_EQUAL_STRING("UNCHANGED", encoded.c_str());
}

// Bereits Header+CRC (41 Bytes ohne Payload) uebersteigt `maxTotalBytes`;
// die Ablehnung darf nicht erst bei der Payloadaddition erfolgen.
void test_envelope_encode_rejects_overflow_already_at_header_plus_crc() {
    auto envelope = validEnvelope();
    envelope.payload.clear();
    std::string encoded = "UNCHANGED";
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeEncodeStatus::CapacityExceeded),
        static_cast<int>(
            device_platform::encodeEnvelope(envelope, encoded, 40U)));
    TEST_ASSERT_EQUAL_STRING("UNCHANGED", encoded.c_str());
}

// `maxTotalBytes` reicht exakt fuer Header+CRC, aber nicht mehr fuer die
// Payload: die Ablehnung muss aus der Payloadaddition stammen.
void test_envelope_encode_rejects_overflow_at_header_plus_payload() {
    auto envelope = validEnvelope();
    envelope.payload = "A";
    std::string encoded = "UNCHANGED";
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::EnvelopeEncodeStatus::CapacityExceeded),
        static_cast<int>(
            device_platform::encodeEnvelope(envelope, encoded, 41U)));
    TEST_ASSERT_EQUAL_STRING("UNCHANGED", encoded.c_str());
}

// `outBytes` darf beim Aufruf bereits einen vollstaendigen alten Record
// enthalten (siehe docs/CONFIGURATION_PERSISTENCE.md, Abschnitt
// "Ressourcenvertrag"): ein erfolgreicher Encode ersetzt ihn vollstaendig per
// `swap()`, nicht nur teilweise oder additiv.
void test_envelope_encode_replaces_preexisting_old_record_on_success() {
    auto oldEnvelope = validEnvelope();
    oldEnvelope.versionValue = 1U;
    oldEnvelope.payload = "OLD-RECORD-PAYLOAD";
    std::string outBytes;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(oldEnvelope, outBytes, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    const std::string oldEncoded = outBytes;
    TEST_ASSERT_FALSE(oldEncoded.empty());

    auto newEnvelope = validEnvelope();
    newEnvelope.versionValue = 2U;
    newEnvelope.payload = "NEW-RECORD-PAYLOAD-DIFFERENT-LENGTH";
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(newEnvelope, outBytes, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);

    // Vollstaendig ersetzt: weder ein Rest des alten Records noch eine
    // Vermischung von altem und neuem Inhalt.
    TEST_ASSERT_TRUE(outBytes != oldEncoded);
    std::string expectedNewEncoded;
    TEST_ASSERT_TRUE(device_platform::encodeEnvelope(
                         newEnvelope, expectedNewEncoded, 1024U) ==
                     device_platform::EnvelopeEncodeStatus::Success);
    TEST_ASSERT_TRUE(outBytes == expectedNewEncoded);
}

// `InvalidField` darf einen bereits in `outBytes` vorhandenen vollstaendigen
// alten Record nicht antasten - nicht nur einen kurzen Platzhalterstring wie
// die aelteren Ablehnungstests oben, sondern einen tatsaechlichen Record.
void test_envelope_encode_leaves_preexisting_old_record_unchanged_on_invalid_field() {
    auto oldEnvelope = validEnvelope();
    std::string outBytes;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(oldEnvelope, outBytes, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    const std::string oldEncoded = outBytes;

    auto invalidEnvelope = validEnvelope();
    invalidEnvelope.versionValue = 0U;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(invalidEnvelope, outBytes, 1024U) ==
        device_platform::EnvelopeEncodeStatus::InvalidField);
    TEST_ASSERT_TRUE(outBytes == oldEncoded);
}

// Gleiches fuer `CapacityExceeded`: der alte vollstaendige Record bleibt
// bestehen, wenn der neue Record nicht in `maxTotalBytes` passt.
void test_envelope_encode_leaves_preexisting_old_record_unchanged_on_capacity_exceeded() {
    auto oldEnvelope = validEnvelope();
    std::string outBytes;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(oldEnvelope, outBytes, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    const std::string oldEncoded = outBytes;

    auto tooLargeEnvelope = validEnvelope();
    tooLargeEnvelope.payload = std::string(1024U, 'x');
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(tooLargeEnvelope, outBytes, 10U) ==
        device_platform::EnvelopeEncodeStatus::CapacityExceeded);
    TEST_ASSERT_TRUE(outBytes == oldEncoded);
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

void test_checked_increment_advances_valid_value_by_one() {
    using device_platform::Revision;
    Revision next;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::CheckedIncrementStatus::Success),
        static_cast<int>(
            device_platform::checkedIncrement(Revision(1U), next)));
    TEST_ASSERT_EQUAL_UINT64(2U, next.value());
}

void test_checked_increment_accepts_value_near_max() {
    using device_platform::Generation;
    Generation next;
    const auto nearMax = Generation(std::numeric_limits<uint64_t>::max() - 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::CheckedIncrementStatus::Success),
        static_cast<int>(device_platform::checkedIncrement(nearMax, next)));
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(),
                             next.value());
}

void test_checked_increment_rejects_max_without_wrap_to_zero() {
    using device_platform::RecordSequence;
    auto out = RecordSequence(77U);
    const auto atMax = RecordSequence(std::numeric_limits<uint64_t>::max());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::CheckedIncrementStatus::Overflow),
        static_cast<int>(device_platform::checkedIncrement(atMax, out)));
    // `out` bleibt unveraendert; insbesondere kein stiller Wrap auf 0.
    TEST_ASSERT_EQUAL_UINT64(77U, out.value());
}

void test_checked_increment_rejects_reserved_zero_as_invalid_current() {
    using device_platform::StorageEpoch;
    auto out = StorageEpoch(99U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::CheckedIncrementStatus::InvalidCurrentValue),
        static_cast<int>(
            device_platform::checkedIncrement(StorageEpoch(0U), out)));
    // `out` bleibt unveraendert; 0 wird nicht still zu 1.
    TEST_ASSERT_EQUAL_UINT64(99U, out.value());
}

// Belegt, dass der generische Baustein fuer alle vier starken
// uint64_t-Zaehlertypen einsetzbar ist, ohne dass sich die Typen vermischen
// lassen (jeder Aufruf bleibt durch das Tag getrennt typisiert).
void test_checked_increment_keeps_strong_types_separate() {
    using device_platform::Generation;
    using device_platform::RecordSequence;
    using device_platform::Revision;
    using device_platform::StorageEpoch;

    StorageEpoch epochOut;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::CheckedIncrementStatus::Success),
        static_cast<int>(
            device_platform::checkedIncrement(StorageEpoch(1U), epochOut)));
    TEST_ASSERT_EQUAL_UINT64(2U, epochOut.value());

    Revision revisionOut;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::CheckedIncrementStatus::Success),
        static_cast<int>(
            device_platform::checkedIncrement(Revision(5U), revisionOut)));
    TEST_ASSERT_EQUAL_UINT64(6U, revisionOut.value());

    Generation generationOut;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::CheckedIncrementStatus::Success),
        static_cast<int>(
            device_platform::checkedIncrement(Generation(9U), generationOut)));
    TEST_ASSERT_EQUAL_UINT64(10U, generationOut.value());

    RecordSequence sequenceOut;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::CheckedIncrementStatus::Success),
        static_cast<int>(device_platform::checkedIncrement(RecordSequence(3U),
                                                           sequenceOut)));
    TEST_ASSERT_EQUAL_UINT64(4U, sequenceOut.value());
}

void test_crc32_iso_hdlc_check_value() {
    // Verbindlicher Pruefwert aus docs/CONFIGURATION_PERSISTENCE.md,
    // Abschnitt "CRC-32/ISO-HDLC".
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926U, device_platform::computeCrc32IsoHdlc(
                                              std::string("123456789")));
}

void test_incremental_crc32_single_chunk_matches_one_shot() {
    using device_platform::Crc32IsoHdlc;
    const std::string data = "123456789";
    Crc32IsoHdlc accumulator;
    TEST_ASSERT_TRUE(accumulator.update(data));
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926U, accumulator.finalize());
}

void test_incremental_crc32_multiple_chunks_matches_contiguous_buffer() {
    using device_platform::Crc32IsoHdlc;
    const std::string full = "123456789";
    const uint32_t expected = device_platform::computeCrc32IsoHdlc(full);

    // Chunkgrenze mitten im "Header"-Teil der Beispieldaten (nach Byte 3).
    Crc32IsoHdlc midHeaderSplit;
    TEST_ASSERT_TRUE(midHeaderSplit.update(full.substr(0U, 3U)));
    TEST_ASSERT_TRUE(midHeaderSplit.update(full.substr(3U)));
    TEST_ASSERT_EQUAL_UINT32(expected, midHeaderSplit.finalize());

    // Chunkgrenze exakt zwischen zwei gleich grossen Haelften (analog zu
    // Header/Payload-Grenze im Envelope).
    Crc32IsoHdlc headerPayloadSplit;
    TEST_ASSERT_TRUE(
        headerPayloadSplit.update(full.substr(0U, full.size() / 2U)));
    TEST_ASSERT_TRUE(headerPayloadSplit.update(full.substr(full.size() / 2U)));
    TEST_ASSERT_EQUAL_UINT32(expected, headerPayloadSplit.finalize());

    // Viele einzelne Ein-Byte-Chunks.
    Crc32IsoHdlc byteByByte;
    for (char character : full) {
        TEST_ASSERT_TRUE(byteByByte.update(std::string(1U, character)));
    }
    TEST_ASSERT_EQUAL_UINT32(expected, byteByByte.finalize());
}

void test_incremental_crc32_empty_chunks_do_not_change_result() {
    using device_platform::Crc32IsoHdlc;
    const std::string full = "123456789";
    const uint32_t expected = device_platform::computeCrc32IsoHdlc(full);

    Crc32IsoHdlc withEmptyChunks;
    TEST_ASSERT_TRUE(withEmptyChunks.update(std::string()));
    TEST_ASSERT_TRUE(withEmptyChunks.update(full.substr(0U, 4U)));
    TEST_ASSERT_TRUE(withEmptyChunks.update(nullptr, 0U));
    TEST_ASSERT_TRUE(withEmptyChunks.update(full.substr(4U)));
    TEST_ASSERT_TRUE(withEmptyChunks.update(std::string()));
    TEST_ASSERT_EQUAL_UINT32(expected, withEmptyChunks.finalize());
}

// `nullptr` mit positiver Laenge wird beobachtbar abgelehnt, ohne den
// Akkumulatorzustand zu veraendern: nach dem abgelehnten Aufruf liefert ein
// nachfolgender gueltiger Chunk denselben Wert wie ein frischer Akkumulator.
void test_incremental_crc32_rejects_null_pointer_with_positive_length_and_state_is_unchanged() {
    using device_platform::Crc32IsoHdlc;
    const std::string data = "123456789";
    const uint32_t expected = device_platform::computeCrc32IsoHdlc(data);

    Crc32IsoHdlc accumulator;
    TEST_ASSERT_FALSE(accumulator.update(nullptr, 5U));
    TEST_ASSERT_TRUE(accumulator.update(data));
    TEST_ASSERT_EQUAL_UINT32(expected, accumulator.finalize());
}

void test_incremental_crc32_empty_input_matches_one_shot() {
    using device_platform::Crc32IsoHdlc;
    Crc32IsoHdlc accumulator;
    TEST_ASSERT_EQUAL_UINT32(
        device_platform::computeCrc32IsoHdlc(std::string()),
        accumulator.finalize());
}

// Envelope mit leerer Payload: bewusst geprueft, weil die CRC-Berechnung nun
// inkrementell ueber Header- und Payload-Chunk erfolgt statt ueber einen
// gemeinsamen Puffer - ein leerer Payload-Chunk darf das Ergebnis nicht
// veraendern.
void test_envelope_round_trip_with_empty_payload() {
    device_platform::StorageEnvelope envelope;
    envelope.recordTypeId = device_platform::RecordTypeId(1U);
    envelope.schemaVersion = 1U;
    envelope.storageEpoch = device_platform::StorageEpoch(1U);
    envelope.versionValue = 1U;
    envelope.payload.clear();
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(envelope, encoded, 1024U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(41U, encoded.size());

    const auto decoded = device_platform::decodeEnvelope(encoded);
    TEST_ASSERT_TRUE(decoded.status ==
                     device_platform::EnvelopeDecodeStatus::Success);
    TEST_ASSERT_TRUE(decoded.envelope->payload.empty());
}

// Maximale in diesem technischen Test verwendete Payloadgroesse: beweist,
// dass die inkrementelle CRC-Berechnung auch fuer einen groesseren, nicht
// trivialen Payload-Chunk mit dem alten Einzelpuffer-Ergebnis
// uebereinstimmt.
void test_envelope_round_trip_with_larger_payload() {
    device_platform::StorageEnvelope envelope;
    envelope.recordTypeId = device_platform::RecordTypeId(1U);
    envelope.schemaVersion = 1U;
    envelope.storageEpoch = device_platform::StorageEpoch(1U);
    envelope.versionValue = 1U;
    envelope.payload = std::string(4096U, 'x');
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(envelope, encoded, 8192U) ==
        device_platform::EnvelopeEncodeStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(41U + 4096U, encoded.size());

    const auto decoded = device_platform::decodeEnvelope(encoded);
    TEST_ASSERT_TRUE(decoded.status ==
                     device_platform::EnvelopeDecodeStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(4096U, decoded.envelope->payload.size());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_byte_writer_rejects_writes_beyond_capacity);
    RUN_TEST(test_byte_writer_zero_length_write_with_null_pointer_is_safe);
    RUN_TEST(test_byte_reader_zero_length_read_with_null_pointer_is_safe);
    RUN_TEST(test_byte_reader_rejects_reads_beyond_available_bytes);
    RUN_TEST(test_byte_writer_rejects_null_pointer_with_positive_length);
    RUN_TEST(test_byte_reader_rejects_null_pointer_with_positive_length);
    RUN_TEST(test_big_endian_unsigned_golden_bytes);
    RUN_TEST(test_big_endian_uint8_golden_values);
    RUN_TEST(
        test_big_endian_uint8_read_from_empty_buffer_fails_and_leaves_output_unchanged);
    RUN_TEST(test_big_endian_signed_two_complement_golden_bytes);
    RUN_TEST(test_big_endian_int8_golden_values);
    RUN_TEST(test_big_endian_int16_golden_values);
    RUN_TEST(test_big_endian_int32_golden_values);
    RUN_TEST(test_big_endian_int64_golden_values);
    RUN_TEST(test_big_endian_signed_read_failure_leaves_output_unchanged);
    RUN_TEST(test_bool_and_optional_tag_accept_only_zero_and_one);
    RUN_TEST(test_binary64_golden_values_round_trip);
    RUN_TEST(test_binary64_negative_zero_is_normalized_on_encode);
    RUN_TEST(test_binary64_decode_rejects_noncanonical_negative_zero);
    RUN_TEST(test_binary64_rejects_nan_and_infinity_both_ways);
    RUN_TEST(test_binary64_decode_failure_leaves_output_unchanged);
    RUN_TEST(test_checked_increment_advances_valid_value_by_one);
    RUN_TEST(test_checked_increment_accepts_value_near_max);
    RUN_TEST(test_checked_increment_rejects_max_without_wrap_to_zero);
    RUN_TEST(test_checked_increment_rejects_reserved_zero_as_invalid_current);
    RUN_TEST(test_checked_increment_keeps_strong_types_separate);
    RUN_TEST(test_crc32_iso_hdlc_check_value);
    RUN_TEST(test_incremental_crc32_single_chunk_matches_one_shot);
    RUN_TEST(test_incremental_crc32_multiple_chunks_matches_contiguous_buffer);
    RUN_TEST(test_incremental_crc32_empty_chunks_do_not_change_result);
    RUN_TEST(test_incremental_crc32_empty_input_matches_one_shot);
    RUN_TEST(
        test_incremental_crc32_rejects_null_pointer_with_positive_length_and_state_is_unchanged);
    RUN_TEST(test_envelope_round_trip_with_empty_payload);
    RUN_TEST(test_envelope_round_trip_with_larger_payload);
    RUN_TEST(test_checked_add_size_accepts_within_bound);
    RUN_TEST(test_checked_add_size_accepts_exact_bound);
    RUN_TEST(test_checked_add_size_rejects_one_over_bound);
    RUN_TEST(test_checked_add_size_rejects_actual_size_t_wraparound);
    RUN_TEST(test_checked_add_size_simulates_32_bit_boundary);
    RUN_TEST(
        test_check_envelope_encoded_size_accepts_payload_exactly_at_uint32_max);
    RUN_TEST(
        test_check_envelope_encoded_size_rejects_payload_one_byte_over_uint32_max_as_capacity_exceeded);
    RUN_TEST(test_envelope_golden_vector_without_utc);
    RUN_TEST(test_envelope_golden_vector_with_utc);
    RUN_TEST(test_envelope_encode_rejects_reserved_zero_fields);
    RUN_TEST(test_envelope_encode_rejects_capacity_overflow);
    RUN_TEST(test_envelope_encode_accepts_exact_max_total_bytes);
    RUN_TEST(test_envelope_encode_rejects_one_byte_over_exact_size);
    RUN_TEST(test_envelope_encode_rejects_overflow_already_at_header_plus_crc);
    RUN_TEST(test_envelope_encode_rejects_overflow_at_header_plus_payload);
    RUN_TEST(test_envelope_encode_replaces_preexisting_old_record_on_success);
    RUN_TEST(
        test_envelope_encode_leaves_preexisting_old_record_unchanged_on_invalid_field);
    RUN_TEST(
        test_envelope_encode_leaves_preexisting_old_record_unchanged_on_capacity_exceeded);
    RUN_TEST(test_envelope_decode_rejects_invalid_magic);
    RUN_TEST(test_envelope_decode_rejects_unknown_envelope_version);
    RUN_TEST(test_envelope_decode_rejects_reserved_zero_fields);
    RUN_TEST(test_envelope_decode_rejects_invalid_utc_tag);
    RUN_TEST(test_envelope_decode_rejects_wrong_and_overflowing_lengths);
    RUN_TEST(test_envelope_decode_rejects_crc_errors_in_header_and_payload);
    RUN_TEST(test_envelope_preserves_unknown_origin_and_operation_ids);
    return UNITY_END();
}
