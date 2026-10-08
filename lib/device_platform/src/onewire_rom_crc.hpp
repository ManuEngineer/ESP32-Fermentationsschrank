#pragma once

#include <cstdint>

#include "ds18b20_bus.hpp"

namespace device_platform {

// Prueft die 1-Wire-ROM-CRC (Dallas/Maxim CRC-8, Polynom x^8+x^5+x^4+1,
// reflektiert 0x8C, Initialwert 0). Bytereihenfolge wie bei ESP-IDF
// `onewire_device_address_t`: niedrigstes Byte = Familiencode, Bytes 0..6
// ergeben die CRC, die im hoechsten Byte (Byte 7) steht.
[[nodiscard]] constexpr bool hasValidOneWireRomCrc(OneWireRom rom) noexcept {
    uint8_t crc = 0U;
    for (unsigned index = 0U; index < 7U; ++index) {
        crc ^= static_cast<uint8_t>(rom >> (8U * index));
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = static_cast<uint8_t>((crc & 1U) != 0U ? (crc >> 1U) ^ 0x8CU
                                                        : (crc >> 1U));
        }
    }
    return crc == static_cast<uint8_t>(rom >> 56U);
}

}  // namespace device_platform
