#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"

// IEEE-754-binary64-Codec, Big Endian. Siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Gleitkommawerte".
namespace device_platform::binary64 {

static_assert(sizeof(double) == 8U,
              "binary64-Codec setzt eine 8-Byte-double-Repraesentation voraus");
static_assert(std::numeric_limits<double>::is_iec559,
              "binary64-Codec setzt IEEE-754-double voraus");
static_assert(std::numeric_limits<double>::radix == 2,
              "binary64-Codec setzt Basis 2 voraus");
static_assert(std::numeric_limits<double>::digits == 53,
              "binary64-Codec setzt 53 signifikante Bits voraus");
static_assert(std::numeric_limits<double>::max_exponent == 1024,
              "binary64-Codec setzt den binary64-Exponentenbereich voraus");

namespace detail {
inline constexpr uint64_t kNegativeZeroBits = 0x8000000000000000ULL;

[[nodiscard]] inline uint64_t toBits(double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

[[nodiscard]] inline double fromBits(uint64_t bits) {
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
}  // namespace detail

// Lehnt NaN und Unendlichkeit ab. Normalisiert `-0.0` zu `+0.0` vor dem
// Schreiben.
[[nodiscard]] inline bool encode(double value, ByteWriter& writer) {
    if (std::isnan(value) || std::isinf(value)) {
        return false;
    }
    if (value == 0.0) {
        value =
            0.0;  // normalisiert -0.0 zu +0.0 (Vorzeichenbit wird geloescht)
    }
    return big_endian::writeUint64(writer, detail::toBits(value));
}

// Lehnt NaN, Unendlichkeit und eine nicht kanonische negative Null ab.
[[nodiscard]] inline bool decode(ByteReader& reader, double& out) {
    uint64_t bits = 0;
    if (!big_endian::readUint64(reader, bits)) {
        return false;
    }
    if (bits == detail::kNegativeZeroBits) {
        return false;
    }
    const double value = detail::fromBits(bits);
    if (std::isnan(value) || std::isinf(value)) {
        return false;
    }
    out = value;
    return true;
}

}  // namespace device_platform::binary64
