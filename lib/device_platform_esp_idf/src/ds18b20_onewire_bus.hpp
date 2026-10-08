#pragma once

#include <array>
#include <memory>

#include "ds18b20_bus.hpp"

// Konkreter ESP-IDF-Adapter des technischen DS18B20-Busports (Issue #30, Plan
// 5.4) auf espressif/onewire_bus (RMT-Backend) und espressif/ds18b20. Ein
// Objekt bedient genau einen 1-Wire-Bus. Keine Rollen, keine IDF-Typen im
// Header. Alle Aufrufe koennen blockieren (Treiberverhalten) und gehoeren nur
// in den Sampling-Task.
struct onewire_bus_t;
struct ds18b20_device_t;

namespace device_platform_esp_idf {

class Ds18b20OnewireBus final : public device_platform::IDs18b20Bus {
   public:
    // Erzeugt den RMT-Bus auf `gpio`; nullptr, wenn der Treiber den Bus nicht
    // anlegen kann (kein Teilnehmer ist dafuer noetig).
    [[nodiscard]] static std::unique_ptr<Ds18b20OnewireBus> create(int gpio);
    ~Ds18b20OnewireBus() override;

    [[nodiscard]] device_platform::Ds18b20DriverResult presence() override;
    [[nodiscard]] device_platform::Ds18b20Enumeration enumerate() override;
    [[nodiscard]] device_platform::Ds18b20DriverResult startConversionAll()
        override;
    [[nodiscard]] device_platform::Ds18b20ScratchpadRead read(
        device_platform::OneWireRom rom) override;
    [[nodiscard]] device_platform::Ds18b20DriverResult setResolution12(
        device_platform::OneWireRom rom) override;

   private:
    struct Handle {
        device_platform::OneWireRom rom{0U};
        ds18b20_device_t* device{nullptr};
    };

    explicit Ds18b20OnewireBus(onewire_bus_t* bus) : bus_(bus) {}
    // Handle fuer ein ROM (wird bei (Wieder-)Erkennung angelegt, hoechstens
    // kMaxOneWireDevicesPerBus; nicht mehr vorhandene ROMs werden bei der
    // Enumeration freigegeben).
    ds18b20_device_t* handleFor(device_platform::OneWireRom rom);
    void pruneHandles(const device_platform::Ds18b20Enumeration& found);

    onewire_bus_t* bus_;
    std::array<Handle, device_platform::kMaxOneWireDevicesPerBus> handles_{};
};

}  // namespace device_platform_esp_idf
