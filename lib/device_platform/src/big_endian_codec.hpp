#pragma once

#include <cstdint>

#include "byte_buffer.hpp"

// Deterministische Big-Endian-Kodierung fester Integerbreiten, signierter
// Zweierkomplementwerte, Boolwerte und Optionaltags. Unabhaengig von
// C++-Layout, Padding, ABI und nativer Enumdarstellung (siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Kanonisches Wireformat").
namespace device_platform::big_endian {

[[nodiscard]] inline bool writeUint8(ByteWriter& writer, uint8_t value) {
    return writer.writeByte(value);
}

[[nodiscard]] inline bool writeUint16(ByteWriter& writer, uint16_t value) {
    const uint8_t bytes[2] = {
        static_cast<uint8_t>(value >> 8U),
        static_cast<uint8_t>(value),
    };
    return writer.writeBytes(bytes, sizeof(bytes));
}

[[nodiscard]] inline bool writeUint32(ByteWriter& writer, uint32_t value) {
    const uint8_t bytes[4] = {
        static_cast<uint8_t>(value >> 24U),
        static_cast<uint8_t>(value >> 16U),
        static_cast<uint8_t>(value >> 8U),
        static_cast<uint8_t>(value),
    };
    return writer.writeBytes(bytes, sizeof(bytes));
}

[[nodiscard]] inline bool writeUint64(ByteWriter& writer, uint64_t value) {
    const uint8_t bytes[8] = {
        static_cast<uint8_t>(value >> 56U), static_cast<uint8_t>(value >> 48U),
        static_cast<uint8_t>(value >> 40U), static_cast<uint8_t>(value >> 32U),
        static_cast<uint8_t>(value >> 24U), static_cast<uint8_t>(value >> 16U),
        static_cast<uint8_t>(value >> 8U),  static_cast<uint8_t>(value),
    };
    return writer.writeBytes(bytes, sizeof(bytes));
}

[[nodiscard]] inline bool writeInt8(ByteWriter& writer, int8_t value) {
    return writeUint8(writer, static_cast<uint8_t>(value));
}
[[nodiscard]] inline bool writeInt16(ByteWriter& writer, int16_t value) {
    return writeUint16(writer, static_cast<uint16_t>(value));
}
[[nodiscard]] inline bool writeInt32(ByteWriter& writer, int32_t value) {
    return writeUint32(writer, static_cast<uint32_t>(value));
}
[[nodiscard]] inline bool writeInt64(ByteWriter& writer, int64_t value) {
    return writeUint64(writer, static_cast<uint64_t>(value));
}

// Nur 0x00/0x01 sind gueltige Boolwerte auf dem Draht.
[[nodiscard]] inline bool writeBool(ByteWriter& writer, bool value) {
    return writer.writeByte(value ? 0x01U : 0x00U);
}

// Nur 0x00/0x01 sind gueltige Optionaltags auf dem Draht.
[[nodiscard]] inline bool writeOptionalTag(ByteWriter& writer, bool present) {
    return writer.writeByte(present ? 0x01U : 0x00U);
}

[[nodiscard]] inline bool readUint8(ByteReader& reader, uint8_t& out) {
    return reader.readByte(out);
}

[[nodiscard]] inline bool readUint16(ByteReader& reader, uint16_t& out) {
    uint8_t bytes[2] = {0, 0};
    if (!reader.readBytes(bytes, sizeof(bytes))) {
        return false;
    }
    out = static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8U) |
                                bytes[1]);
    return true;
}

[[nodiscard]] inline bool readUint32(ByteReader& reader, uint32_t& out) {
    uint8_t bytes[4] = {0, 0, 0, 0};
    if (!reader.readBytes(bytes, sizeof(bytes))) {
        return false;
    }
    out = (static_cast<uint32_t>(bytes[0]) << 24U) |
          (static_cast<uint32_t>(bytes[1]) << 16U) |
          (static_cast<uint32_t>(bytes[2]) << 8U) |
          static_cast<uint32_t>(bytes[3]);
    return true;
}

[[nodiscard]] inline bool readUint64(ByteReader& reader, uint64_t& out) {
    uint8_t bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    if (!reader.readBytes(bytes, sizeof(bytes))) {
        return false;
    }
    out = (static_cast<uint64_t>(bytes[0]) << 56U) |
          (static_cast<uint64_t>(bytes[1]) << 48U) |
          (static_cast<uint64_t>(bytes[2]) << 40U) |
          (static_cast<uint64_t>(bytes[3]) << 32U) |
          (static_cast<uint64_t>(bytes[4]) << 24U) |
          (static_cast<uint64_t>(bytes[5]) << 16U) |
          (static_cast<uint64_t>(bytes[6]) << 8U) |
          static_cast<uint64_t>(bytes[7]);
    return true;
}

[[nodiscard]] inline bool readInt8(ByteReader& reader, int8_t& out) {
    uint8_t raw = 0;
    if (!readUint8(reader, raw)) {
        return false;
    }
    out = static_cast<int8_t>(raw);
    return true;
}

[[nodiscard]] inline bool readInt16(ByteReader& reader, int16_t& out) {
    uint16_t raw = 0;
    if (!readUint16(reader, raw)) {
        return false;
    }
    out = static_cast<int16_t>(raw);
    return true;
}

[[nodiscard]] inline bool readInt32(ByteReader& reader, int32_t& out) {
    uint32_t raw = 0;
    if (!readUint32(reader, raw)) {
        return false;
    }
    out = static_cast<int32_t>(raw);
    return true;
}

[[nodiscard]] inline bool readInt64(ByteReader& reader, int64_t& out) {
    uint64_t raw = 0;
    if (!readUint64(reader, raw)) {
        return false;
    }
    out = static_cast<int64_t>(raw);
    return true;
}

// Liefert `false`, wenn das gelesene Byte weder 0x00 noch 0x01 ist; `out`
// bleibt in diesem Fall unveraendert.
[[nodiscard]] inline bool readBool(ByteReader& reader, bool& out) {
    uint8_t raw = 0;
    if (!readUint8(reader, raw)) {
        return false;
    }
    if (raw != 0x00U && raw != 0x01U) {
        return false;
    }
    out = raw == 0x01U;
    return true;
}

// Liefert `false`, wenn das gelesene Byte weder 0x00 noch 0x01 ist; `out`
// bleibt in diesem Fall unveraendert.
[[nodiscard]] inline bool readOptionalTag(ByteReader& reader, bool& out) {
    return readBool(reader, out);
}

}  // namespace device_platform::big_endian
