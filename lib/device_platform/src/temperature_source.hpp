#pragma once

namespace device_platform {

// Rohmesswert einer einzelnen Temperaturfuehlerrolle. `available` bildet nur
// Bus-/Kommunikationsfehler auf Portebene ab (zum Beispiel CRC- oder
// Busausfall). Die fachliche Qualitaetsklassifikation VALID/STALE/FAILED samt
// Filterung ist keine Aufgabe dieses Ports.
struct TemperatureReading {
    bool available;
    double celsius;
};

// Anwendungsneutraler Port fuer eine einzelne Temperaturfuehlerrolle. Eine
// Instanz vertritt genau eine Rolle; welche Rolle das ist und was sie fachlich
// bedeutet, entscheidet ausschliesslich die Anwendung ueber die Zuordnung der
// konkreten Instanz.
class ITemperatureSource {
   public:
    ITemperatureSource() = default;
    virtual ~ITemperatureSource() = default;

    ITemperatureSource(const ITemperatureSource&) = delete;
    ITemperatureSource& operator=(const ITemperatureSource&) = delete;
    ITemperatureSource(ITemperatureSource&&) = delete;
    ITemperatureSource& operator=(ITemperatureSource&&) = delete;

    [[nodiscard]] virtual TemperatureReading read() const = 0;
};

}  // namespace device_platform
