#pragma once

#include <cstdint>
#include <map>
#include <optional>

#include "ds18b20_bus.hpp"

namespace device_platform_test_support {

// Skriptbarer Bus fuer die native Matrix der DS18B20-Sampling-Engine. Kein
// Hardware- oder Zeitzugriff. `read()` liefert den bei der letzten
// Konvertierung eingefrorenen Wert; ein Lesen ohne frische Konvertierung seit
// dem letzten Lesen wird gezaehlt (`staleReadCount()`), damit Tests belegen
// koennen, dass nie eine alte Messung als neu gelesen wird.
class FakeDs18b20Bus final : public device_platform::IDs18b20Bus {
   public:
    void addDevice(device_platform::OneWireRom rom, double celsius);
    void removeDevice(device_platform::OneWireRom rom);
    void setCelsius(device_platform::OneWireRom rom, double celsius);
    // Dauerhafte Fehlersicht je Teilnehmer fuer read(); Ok hebt auf.
    void setReadResult(device_platform::OneWireRom rom,
                       device_platform::Ds18b20DriverResult result);
    void setSetResolutionResult(device_platform::Ds18b20DriverResult result);
    // Ueberschreibt das aus den Teilnehmern abgeleitete Ergebnis.
    void overridePresence(
        std::optional<device_platform::Ds18b20DriverResult> result);
    void overrideEnumeration(
        std::optional<device_platform::Ds18b20DriverResult> result);
    void overrideStartConversion(
        std::optional<device_platform::Ds18b20DriverResult> result);

    [[nodiscard]] device_platform::Ds18b20DriverResult presence() override;
    [[nodiscard]] device_platform::Ds18b20Enumeration enumerate() override;
    [[nodiscard]] device_platform::Ds18b20DriverResult startConversionAll()
        override;
    [[nodiscard]] device_platform::Ds18b20ScratchpadRead read(
        device_platform::OneWireRom rom) override;
    [[nodiscard]] device_platform::Ds18b20DriverResult setResolution12(
        device_platform::OneWireRom rom) override;

    [[nodiscard]] uint32_t conversionCount() const { return conversions_; }
    [[nodiscard]] uint32_t readCount() const { return reads_; }
    [[nodiscard]] uint32_t staleReadCount() const { return staleReads_; }
    [[nodiscard]] uint32_t resolutionCalls() const { return resolutionCalls_; }
    [[nodiscard]] uint32_t enumerateCount() const { return enumerations_; }

   private:
    struct Device {
        double celsius{0.0};
        double latched{0.0};
        bool freshSinceRead{false};
        device_platform::Ds18b20DriverResult readResult{
            device_platform::Ds18b20DriverResult::Ok};
    };

    std::map<device_platform::OneWireRom, Device> devices_;
    std::optional<device_platform::Ds18b20DriverResult> presenceOverride_;
    std::optional<device_platform::Ds18b20DriverResult> enumerationOverride_;
    std::optional<device_platform::Ds18b20DriverResult> startOverride_;
    device_platform::Ds18b20DriverResult setResolutionResult_{
        device_platform::Ds18b20DriverResult::Ok};
    uint32_t conversions_{0};
    uint32_t reads_{0};
    uint32_t staleReads_{0};
    uint32_t resolutionCalls_{0};
    uint32_t enumerations_{0};
};

}  // namespace device_platform_test_support
