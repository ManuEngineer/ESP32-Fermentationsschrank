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
// Puffer zusammenzufuegen. `encodeEnvelope()`/`decodeEnvelope()` nutzen dies,
// um ohne zusaetzlichen Verkettungspuffer auszukommen (siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Ressourcenvertrag" fuer die
// praezise, nicht absolute Formulierung der dortigen Ein-Puffer-Garantie -
// diese Klasse selbst macht keine Aussage ueber gleichzeitig existierende
// Puffer). Ergebnis ist unabhaengig von der Chunkaufteilung identisch zum
// CRC ueber den zusammenhaengenden Bereich.
class Crc32IsoHdlc {
   public:
    Crc32IsoHdlc() = default;

    // Laenge 0: `data` darf `nullptr` sein, der Aufruf ist ein erfolgreicher
    // No-op und veraendert den Akkumulatorzustand nicht. Positive Laenge:
    // `nullptr` wird beobachtbar mit `false` abgelehnt, ohne `data` zu
    // dereferenzieren und ohne den Akkumulatorzustand zu veraendern.
    [[nodiscard]] bool update(const void* data, std::size_t length);
    [[nodiscard]] bool update(const std::string& data);

    [[nodiscard]] uint32_t finalize() const;

   private:
    uint32_t crc_{0xFFFFFFFFU};
};

// Bequemer Einmalaufruf fuer den haeufigen Fall vollstaendig im Speicher
// vorliegender `std::string`-Daten; `std::string::data()` ist nie `nullptr`
// (auch bei leerem String seit C++11), dieser Aufruf kann daher nie
// fehlschlagen. Fuer rohe Zeiger/Laengen oder mehrere Chunks direkt
// `Crc32IsoHdlc` verwenden.
[[nodiscard]] uint32_t computeCrc32IsoHdlc(const std::string& data);

}  // namespace device_platform
