#include "ds18b20_onewire_bus.hpp"

#include <cstring>

#include "ds18b20.h"
#include "esp_err.h"
#include "onewire_bus.h"
#include "onewire_cmd.h"
#include "onewire_device.h"

namespace device_platform_esp_idf {
namespace {

using device_platform::Ds18b20DriverResult;

// DS18B20 "Convert T" nach SKIP ROM (alle Teilnehmer des Busses).
constexpr uint8_t kConvertTCommand = 0x44U;

Ds18b20DriverResult mapEspError(esp_err_t error) {
    switch (error) {
        case ESP_OK:
            return Ds18b20DriverResult::Ok;
        case ESP_ERR_NOT_FOUND:
            return Ds18b20DriverResult::NotFound;
        case ESP_ERR_TIMEOUT:
            return Ds18b20DriverResult::Timeout;
        case ESP_ERR_INVALID_CRC:
            return Ds18b20DriverResult::InvalidCrc;
        case ESP_ERR_INVALID_STATE:
            // ds18b20_get_temperature: Einschaltwert 85,0 Grad Celsius.
            return Ds18b20DriverResult::PowerOnValue;
        default:
            return Ds18b20DriverResult::Other;
    }
}

}  // namespace

std::unique_ptr<Ds18b20OnewireBus> Ds18b20OnewireBus::create(int gpio) {
    onewire_bus_config_t busConfig{};
    busConfig.bus_gpio_num = gpio;
    busConfig.flags.en_pull_up = 0U;  // externer 4,7-kOhm-Pull-up laut SSOT
    // Espressif-Beispiel: 1 Byte ROM-Kommando + 8 Byte ROM + 1 Byte Kommando.
    onewire_bus_rmt_config_t rmtConfig{};
    rmtConfig.max_rx_bytes = 10U;
    onewire_bus_handle_t bus = nullptr;
    if (onewire_new_bus_rmt(&busConfig, &rmtConfig, &bus) != ESP_OK ||
        bus == nullptr) {
        return nullptr;
    }
    return std::unique_ptr<Ds18b20OnewireBus>(new Ds18b20OnewireBus(bus));
}

Ds18b20OnewireBus::~Ds18b20OnewireBus() {
    for (auto& handle : handles_) {
        if (handle.device != nullptr) {
            static_cast<void>(ds18b20_del_device(handle.device));
        }
    }
    static_cast<void>(onewire_bus_del(bus_));
}

Ds18b20DriverResult Ds18b20OnewireBus::presence() {
    return mapEspError(onewire_bus_reset(bus_));
}

device_platform::Ds18b20Enumeration Ds18b20OnewireBus::enumerate() {
    device_platform::Ds18b20Enumeration result;
    onewire_device_iter_handle_t iterator = nullptr;
    if (onewire_new_device_iter(bus_, &iterator) != ESP_OK ||
        iterator == nullptr) {
        result.result = Ds18b20DriverResult::Other;
        return result;
    }
    result.result = Ds18b20DriverResult::Ok;
    // Begrenzte Suche: hoechstens kMax + 1 Teilnehmer (der letzte setzt nur
    // overflow), damit eine fehlerhafte Busbelegung die Suche nicht ausdehnt.
    for (std::size_t guard = 0U;
         guard <= device_platform::kMaxOneWireDevicesPerBus; ++guard) {
        onewire_device_t device{};
        const esp_err_t next = onewire_device_iter_get_next(iterator, &device);
        if (next == ESP_ERR_NOT_FOUND) break;  // Ende der Liste, kein Fehler
        if (next != ESP_OK) {
            result.result = mapEspError(next);
            break;
        }
        if (result.count < device_platform::kMaxOneWireDevicesPerBus) {
            result.roms[result.count++] = device.address;
        } else {
            result.overflow = true;
            break;
        }
    }
    static_cast<void>(onewire_del_device_iter(iterator));
    if (result.result == Ds18b20DriverResult::Ok) pruneHandles(result);
    return result;
}

Ds18b20DriverResult Ds18b20OnewireBus::startConversionAll() {
    const esp_err_t reset = onewire_bus_reset(bus_);
    if (reset != ESP_OK) return mapEspError(reset);
    const uint8_t command[2] = {ONEWIRE_CMD_SKIP_ROM, kConvertTCommand};
    return mapEspError(onewire_bus_write_bytes(bus_, command, sizeof(command)));
}

ds18b20_device_t* Ds18b20OnewireBus::handleFor(
    device_platform::OneWireRom rom) {
    for (auto& handle : handles_) {
        if (handle.device != nullptr && handle.rom == rom) return handle.device;
    }
    for (auto& handle : handles_) {
        if (handle.device != nullptr) continue;
        onewire_device_t device{};
        device.bus = bus_;
        device.address = rom;
        ds18b20_config_t config{};
        ds18b20_device_handle_t created = nullptr;
        if (ds18b20_new_device_from_enumeration(&device, &config, &created) !=
                ESP_OK ||
            created == nullptr) {
            return nullptr;
        }
        handle.rom = rom;
        handle.device = created;
        return created;
    }
    return nullptr;
}

void Ds18b20OnewireBus::pruneHandles(
    const device_platform::Ds18b20Enumeration& found) {
    for (auto& handle : handles_) {
        if (handle.device == nullptr) continue;
        bool present = false;
        for (uint8_t i = 0U; i < found.count; ++i) {
            present = present || found.roms[i] == handle.rom;
        }
        if (!present) {
            static_cast<void>(ds18b20_del_device(handle.device));
            handle = Handle{};
        }
    }
}

device_platform::Ds18b20ScratchpadRead Ds18b20OnewireBus::read(
    device_platform::OneWireRom rom) {
    device_platform::Ds18b20ScratchpadRead result;
    ds18b20_device_t* device = handleFor(rom);
    if (device == nullptr) {
        result.result = Ds18b20DriverResult::Other;
        return result;
    }
    float celsius = 0.0F;
    result.result = mapEspError(ds18b20_get_temperature(device, &celsius));
    if (result.result == Ds18b20DriverResult::Ok) {
        result.celsius = static_cast<double>(celsius);
    }
    return result;
}

Ds18b20DriverResult Ds18b20OnewireBus::setResolution12(
    device_platform::OneWireRom rom) {
    ds18b20_device_t* device = handleFor(rom);
    if (device == nullptr) return Ds18b20DriverResult::Other;
    return mapEspError(ds18b20_set_resolution(device, DS18B20_RESOLUTION_12B));
}

}  // namespace device_platform_esp_idf
