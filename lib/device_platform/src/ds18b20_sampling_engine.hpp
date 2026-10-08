#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>

#include "ds18b20_bus.hpp"
#include "sensor_identity.hpp"
#include "temperature_source.hpp"
#include "time_source.hpp"

// Zyklus- und Bindungsautomat fuer zwei DS18B20-Busse (ADR-013: nur technische
// Kanaele, keine Rollen; siehe
// docs/tasks/issue-30-ds18b20-sensor-adapters-plan.md, Abschnitte 4, 4a, 4b, 4c
// und 5.2).
//
//   Bus 0, Betriebsart ExpectedRoms: Mehrteilnehmerbus mit den Kanaelen 0 und
//   1;
//          jeder Kanal ist an ein erwartetes ROM gebunden.
//   Bus 1, Betriebsart SingleDevice: genau ein austauschbares Geraet, Kanal 2.
//
// Die Engine trifft keine fachliche Entscheidung: keine Anzahl guter Proben,
// keine VALID-Schwelle, keine Presence-Entprellung. Jeder Zyklus meldet das
// tatsaechliche technische Ergebnis; Recovery, Plausibilitaet und
// ROM-Wechsel-Erkennung liegen ausschliesslich in SensorQualityPipeline (#20).
namespace device_platform {

struct Ds18b20ChannelBinding {
    uint8_t channel{0};
    OneWireRom expectedRom{0};
};

struct Ds18b20BusReport {
    Ds18b20DriverResult presence{Ds18b20DriverResult::Other};
    Ds18b20Enumeration enumeration;
    // Nur Bus 0: unbekanntes zusaetzliches ROM oder Overflow.
    bool bindingConflict{false};
};

struct Ds18b20EnumerationReport {
    bool bindingValid{false};
    Ds18b20BusReport expectedRomBus;
    Ds18b20BusReport singleDeviceBus;
    // Zuletzt auf dem SingleDevice-Bus gesehenes einzelnes ROM seit dem Boot.
    std::optional<OneWireRom> singleDeviceLastSeen;
};

class Ds18b20SamplingEngine {
   public:
    static constexpr uint8_t kExpectedRomChannelCount = 2U;
    static constexpr uint8_t kSingleDeviceChannel = 2U;
    static constexpr uint8_t kChannelCount = 3U;
    static constexpr uint64_t kCyclePeriodMs = 2'000U;
    // Konvertierungszeit fuer 12 Bit (Datenblatt maximal 750 ms; Treiberwert).
    static constexpr uint64_t kConversionWaitMs = 800U;

    Ds18b20SamplingEngine(const ITimeSource& time, IDs18b20Bus& expectedRomBus,
                          IDs18b20Bus& singleDeviceBus);

    // Setzt die ROM-Bindung der Kanaele 0 und 1. Gueltig nur bei genau einem
    // Eintrag je Kanal 0 und 1, ROM != 0 und paarweise verschieden; sonst
    // bleiben beide Kanaele ungebunden (nie Ok). Wird beim Boot gesetzt.
    void setBinding(const std::array<std::optional<Ds18b20ChannelBinding>,
                                     kExpectedRomChannelCount>& bindings);

    // Fuehrt bei Faelligkeit die Busarbeit durch (BLOCKIERT, nur im
    // Sampling-Task oder in Tests aufrufen).
    void step();
    [[nodiscard]] uint64_t nextDueMillis() const;
    [[nodiscard]] uint64_t completedCycles() const;

    // Thread-sicherer Stand des letzten veroeffentlichten Readings.
    [[nodiscard]] TemperatureReading reading(uint8_t channel) const;
    [[nodiscard]] Ds18b20EnumerationReport report() const;

   private:
    enum class Phase : uint8_t { Idle, WaitingForConversion };

    struct ChannelPlan {
        bool convert{false};
        OneWireRom rom{0};
    };

    void beginCycle(uint64_t now);
    void finishCycle();
    void planExpectedRomBus(uint64_t now);
    void planSingleDeviceBus(uint64_t now);
    void publish(uint8_t channel, std::optional<OneWireRom> identity,
                 TemperatureSampleStatus status, std::optional<double> celsius,
                 uint64_t timestampMs);
    void publishFault(uint8_t channel, std::optional<OneWireRom> identity,
                      Ds18b20DriverResult result, uint64_t timestampMs);
    [[nodiscard]] std::optional<OneWireRom> expectedRomOf(
        uint8_t channel) const;
    [[nodiscard]] bool resolutionKnown(std::size_t bus, OneWireRom rom) const;
    void markResolution(std::size_t bus, OneWireRom rom);
    void forgetResolution(std::size_t bus);
    void pruneResolution(std::size_t bus,
                         const Ds18b20Enumeration& enumeration);

    const ITimeSource& time_;
    std::array<IDs18b20Bus*, 2> buses_;
    std::array<std::optional<OneWireRom>, kExpectedRomChannelCount> binding_{};
    bool bindingValid_{false};

    Phase phase_{Phase::Idle};
    uint64_t cycleStartMs_{0};
    uint64_t nextCycleStartMs_{0};
    uint64_t conversionStartedMs_{0};
    uint64_t completedCycles_{0};
    std::array<ChannelPlan, kChannelCount> plan_{};
    std::array<bool, 2> busTriggered_{};

    // Zuletzt konfigurierte Aufloesung je Bus (reset bei Verlust des Busses
    // bzw. Verschwinden des ROMs).
    std::array<std::array<OneWireRom, kMaxOneWireDevicesPerBus>, 2>
        resolutionSet_{};
    std::optional<OneWireRom> singleDeviceLastSeen_;
    Ds18b20EnumerationReport workingReport_;

    mutable std::mutex mutex_;
    std::array<std::optional<TemperatureReading>, kChannelCount> readings_;
    Ds18b20EnumerationReport report_;
};

}  // namespace device_platform
