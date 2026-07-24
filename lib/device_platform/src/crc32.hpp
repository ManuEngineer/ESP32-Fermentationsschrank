#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// CRC-32/ISO-HDLC: Polynom 0x04C11DB7 (reflektiert 0xEDB88320), Initialwert
// 0xFFFFFFFF, Ein- und Ausgabe reflektiert, finales XOR 0xFFFFFFFF.
// Pruefwert fuer ASCII "123456789": 0xCBF43926. Katalogisierte Residue:
// 0xDEBB20E3. Siehe docs/CONFIGURATION_PERSISTENCE.md, Abschnitt
// "CRC-32/ISO-HDLC". Weder Manipulationsschutz noch Authentifizierung oder
// Verschluesselung.
namespace device_platform {

[[nodiscard]] uint32_t computeCrc32IsoHdlc(const void* data,
                                           std::size_t length);
[[nodiscard]] uint32_t computeCrc32IsoHdlc(const std::string& data);

}  // namespace device_platform
