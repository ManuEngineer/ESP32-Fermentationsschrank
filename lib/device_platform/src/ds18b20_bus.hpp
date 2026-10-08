#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// Technischer 1-Wire/DS18B20-Busport. Anwendungsneutral (ADR-013): keine
// IDF-Typen und keine Rollen- oder Fermentationsbegriffe; die Plattform kennt
// nur Busse, ROM-Adressen und Kanalindizes (siehe
// docs/tasks/issue-30-ds18b20-sensor-adapters-plan.md, Abschnitt 4c/5.2).
namespace device_platform {

// 64-Bit-ROM-Adresse eines 1-Wire-Teilnehmers. 0 ist kein gueltiges ROM.
using OneWireRom = uint64_t;

// Mehr Teilnehmer als diese Grenze gelten als Fehlanschluss (`overflow`);
// die Grenze bleibt ein fester kleiner Puffer ohne Heap im Betrieb.
inline constexpr std::size_t kMaxOneWireDevicesPerBus = 4U;

enum class Ds18b20DriverResult : uint8_t {
    Ok,
    // Kein Presence-Puls bzw. kein Teilnehmer auf dem Bus.
    NotFound,
    Timeout,
    InvalidCrc,
    // Einschaltwert 85,0 Grad Celsius (kein Messwert).
    PowerOnValue,
    Other,
};

struct Ds18b20Enumeration {
    Ds18b20DriverResult result{Ds18b20DriverResult::Other};
    std::array<OneWireRom, kMaxOneWireDevicesPerBus> roms{};
    uint8_t count{0};
    // Mehr als kMaxOneWireDevicesPerBus Teilnehmer.
    bool overflow{false};
};

struct Ds18b20ScratchpadRead {
    Ds18b20DriverResult result{Ds18b20DriverResult::Other};
    // Gesetzt genau dann, wenn result == Ok.
    std::optional<double> celsius;
};

// Ein 1-Wire-Bus. Alle Aufrufe koennen den Aufrufer blockieren (Treiber-
// verhalten); sie duerfen nur aus dem Sampling-Task bzw. aus Tests aufgerufen
// werden, nie aus der Hauptschleife.
class IDs18b20Bus {
   public:
    IDs18b20Bus() = default;
    virtual ~IDs18b20Bus() = default;

    IDs18b20Bus(const IDs18b20Bus&) = delete;
    IDs18b20Bus& operator=(const IDs18b20Bus&) = delete;
    IDs18b20Bus(IDs18b20Bus&&) = delete;
    IDs18b20Bus& operator=(IDs18b20Bus&&) = delete;

    // Reset mit Presence-Erkennung: Ok, NotFound (kein Teilnehmer) oder
    // Timeout/Other (Busfehler).
    [[nodiscard]] virtual Ds18b20DriverResult presence() = 0;
    [[nodiscard]] virtual Ds18b20Enumeration enumerate() = 0;
    // Startet die Konvertierung aller Teilnehmer des Busses; der Aufrufer
    // wartet die Konvertierungszeit ab.
    [[nodiscard]] virtual Ds18b20DriverResult startConversionAll() = 0;
    [[nodiscard]] virtual Ds18b20ScratchpadRead read(OneWireRom rom) = 0;
    [[nodiscard]] virtual Ds18b20DriverResult setResolution12(
        OneWireRom rom) = 0;
};

}  // namespace device_platform
