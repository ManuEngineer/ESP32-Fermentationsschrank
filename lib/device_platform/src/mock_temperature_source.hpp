#pragma once

#include "temperature_source.hpp"

namespace device_platform {

// Deterministisch steuerbarer Mock fuer eine Temperaturfuehlerrolle. Tests und
// das thermische Simulationsmodell setzen den Messwert direkt; es findet kein
// realer Bus- oder Zeitzugriff statt.
class MockTemperatureSource final : public ITemperatureSource {
   public:
    explicit MockTemperatureSource(double initialCelsius);

    [[nodiscard]] TemperatureReading read() const override;

    void setCelsius(double celsius);

    // Simuliert einen Bus-/CRC-Ausfall dieser Fuehlerrolle. Waehrend nicht
    // verfuegbar liefert `read()` `available = false`.
    void setAvailable(bool available);

   private:
    double celsius_;
    bool available_{true};
};

}  // namespace device_platform
