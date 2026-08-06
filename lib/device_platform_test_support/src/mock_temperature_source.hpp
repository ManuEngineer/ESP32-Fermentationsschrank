#pragma once

#include <cstdint>
#include <optional>

#include "sensor_identity.hpp"
#include "temperature_source.hpp"

namespace device_platform_test_support {

// Deterministisch steuerbarer Mock fuer eine Temperaturfuehlerrolle. Tests
// und das thermische Simulationsmodell setzen die Probe direkt; es findet
// kein realer Bus- oder Zeitzugriff statt.
class MockTemperatureSource final : public device_platform::ITemperatureSource {
   public:
    MockTemperatureSource(
        std::optional<device_platform::SensorIdentity> identity,
        uint64_t monotonicTimestampMs, double celsius);

    [[nodiscard]] device_platform::TemperatureReading read() const override;

    // Setzt eine erfolgreiche Messung (status == Ok).
    void setReading(std::optional<device_platform::SensorIdentity> identity,
                    uint64_t monotonicTimestampMs, double celsius);

    // Simuliert einen Mess-/Transportfehler dieser Fuehlerrolle (status !=
    // Ok, kein Messwert). Waehrend eines Fehlers liefert `read()` diese
    // fehlerhafte Probe. status == Ok ist ein Aufruferfehler (eine
    // erfolgreiche Messung gehoert zu setReading()); der Aufruf laesst die
    // zuvor gesetzte Probe in diesem Fall deterministisch unveraendert,
    // statt unkontrolliert fehlzuschlagen.
    void setFault(std::optional<device_platform::SensorIdentity> identity,
                  uint64_t monotonicTimestampMs,
                  device_platform::TemperatureSampleStatus status);

   private:
    device_platform::TemperatureReading reading_;
};

}  // namespace device_platform_test_support
