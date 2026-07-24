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

// Inkrementeller CRC-32/ISO-HDLC-Akkumulator. Erlaubt, den CRC ueber mehrere
// nicht zusammenhaengende Speicherbereiche (z. B. Header und Payload eines
// Envelopes) zu berechnen, ohne diese Bereiche vorher in einem gemeinsamen
// Puffer zusammenzufuegen (siehe docs/CONFIGURATION_PERSISTENCE.md,
// Abschnitt "Ressourcenvertrag": hoechstens ein vollstaendiger kodierter
// Recordpuffer waehrend Commit). Ergebnis ist unabhaengig von der
// Chunkaufteilung identisch zum CRC ueber den zusammenhaengenden Bereich.
class Crc32IsoHdlc {
   public:
    Crc32IsoHdlc() = default;

    // `data` darf nur dann `nullptr` sein, wenn `length == 0` ist (gleiche
    // Vorbedingung wie bei `std::memcpy`/`ByteWriter::writeBytes`); bei
    // `length == 0` wird `data` nicht dereferenziert.
    void update(const void* data, std::size_t length);
    void update(const std::string& data);

    [[nodiscard]] uint32_t finalize() const;

   private:
    uint32_t crc_{0xFFFFFFFFU};
};

[[nodiscard]] uint32_t computeCrc32IsoHdlc(const void* data,
                                           std::size_t length);
[[nodiscard]] uint32_t computeCrc32IsoHdlc(const std::string& data);

}  // namespace device_platform
