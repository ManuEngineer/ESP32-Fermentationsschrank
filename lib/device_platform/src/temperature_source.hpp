#pragma once

#include <cstdint>
#include <optional>

#include "sensor_identity.hpp"

namespace device_platform {

// Generischer Mess-/Transportstatus einer Probe. Enthaelt bewusst KEINE
// sensor-/treiberspezifischen Zahlenkonstanten (siehe
// docs/tasks/issue-20-sensor-quality-filtering-plan.md, Abschnitt 10.1): ein
// Adapter (z. B. der spaetere DS18B20-Adapter aus #30) erkennt einen ihm
// bekannten ungueltigen Rohwert selbst und bildet ihn auf
// KnownInvalidMeasurement ab.
enum class TemperatureSampleStatus : uint8_t {
    Ok,
    BusFault,
    CrcFault,
    MissingSample,
    // Der Adapter hat den Rohwert bereits selbst als einen ihm bekannten
    // ungueltigen Messwert erkannt (z. B. einen Sensor-/Treiber-spezifischen
    // Einschalt- oder Diskonnektwert). Die konkrete Erkennung bleibt Aufgabe
    // des jeweiligen Adapters.
    KnownInvalidMeasurement,
};

enum class TemperatureReadingStatus : uint8_t {
    Success,
    // status == Ok, aber celsius fehlt, ODER status != Ok, aber celsius ist
    // gesetzt. Genau eine der beiden Kombinationen ist gueltig.
    InconsistentValuePresence,
};

// Vorwaertsdeklariert, da sie ein std::optional<TemperatureReading> enthaelt.
struct TemperatureReadingCreateResult;

// Kanonischer Rohprobenvertrag: gueltig-by-construction erzwingt
//   status == Ok             <=> celsius().has_value() == true
//   status != Ok             <=> celsius().has_value() == false
// identity ist unabhaengig davon optional (nullopt = dem Adapter noch nicht
// bekannt, z. B. vor erster Busenumeration); wenn gesetzt, ist es bereits
// eine gueltige SensorIdentity (kein 0-Sentinel moeglich).
class TemperatureReading {
   public:
    [[nodiscard]] static TemperatureReadingCreateResult create(
        std::optional<SensorIdentity> identity, uint64_t monotonicTimestampMs,
        TemperatureSampleStatus status, std::optional<double> celsius);

    [[nodiscard]] std::optional<SensorIdentity> identity() const {
        return identity_;
    }
    [[nodiscard]] uint64_t monotonicTimestampMs() const {
        return monotonicTimestampMs_;
    }
    [[nodiscard]] TemperatureSampleStatus status() const { return status_; }
    [[nodiscard]] std::optional<double> celsius() const { return celsius_; }

   private:
    TemperatureReading(std::optional<SensorIdentity> identity,
                        uint64_t monotonicTimestampMs,
                        TemperatureSampleStatus status,
                        std::optional<double> celsius)
        : identity_(identity),
          monotonicTimestampMs_(monotonicTimestampMs),
          status_(status),
          celsius_(celsius) {}

    std::optional<SensorIdentity> identity_;
    uint64_t monotonicTimestampMs_;
    TemperatureSampleStatus status_;
    std::optional<double> celsius_;
};

struct TemperatureReadingCreateResult {
    TemperatureReadingStatus status{
        TemperatureReadingStatus::InconsistentValuePresence};
    std::optional<TemperatureReading> reading;
};

inline TemperatureReadingCreateResult TemperatureReading::create(
    std::optional<SensorIdentity> identity, uint64_t monotonicTimestampMs,
    TemperatureSampleStatus status, std::optional<double> celsius) {
    const bool celsiusPresent = celsius.has_value();
    const bool statusIsOk = (status == TemperatureSampleStatus::Ok);
    if (statusIsOk != celsiusPresent) {
        return TemperatureReadingCreateResult{
            TemperatureReadingStatus::InconsistentValuePresence,
            std::nullopt};
    }
    return TemperatureReadingCreateResult{
        TemperatureReadingStatus::Success,
        TemperatureReading(identity, monotonicTimestampMs, status, celsius)};
}

// Anwendungsneutraler Port fuer eine einzelne Temperaturfuehlerrolle. Eine
// Instanz vertritt genau eine Rolle; welche Rolle das ist und was sie
// fachlich bedeutet, entscheidet ausschliesslich die Anwendung ueber die
// Zuordnung der konkreten Instanz. Signatur unveraendert gegenueber der
// bisherigen Fassung - nur der Rueckgabewerttyp TemperatureReading ist jetzt
// reichhaltiger.
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
