#include "fake_ds18b20_bus.hpp"

namespace device_platform_test_support {

using device_platform::Ds18b20DriverResult;
using device_platform::OneWireRom;

void FakeDs18b20Bus::addDevice(OneWireRom rom, double celsius) {
    Device device;
    device.celsius = celsius;
    device.latched = celsius;
    devices_[rom] = device;
}

void FakeDs18b20Bus::removeDevice(OneWireRom rom) { devices_.erase(rom); }

void FakeDs18b20Bus::setCelsius(OneWireRom rom, double celsius) {
    const auto found = devices_.find(rom);
    if (found != devices_.end()) found->second.celsius = celsius;
}

void FakeDs18b20Bus::setReadResult(OneWireRom rom, Ds18b20DriverResult result) {
    const auto found = devices_.find(rom);
    if (found != devices_.end()) found->second.readResult = result;
}

void FakeDs18b20Bus::setSetResolutionResult(Ds18b20DriverResult result) {
    setResolutionResult_ = result;
}

void FakeDs18b20Bus::overridePresence(std::optional<Ds18b20DriverResult> r) {
    presenceOverride_ = r;
}

void FakeDs18b20Bus::overrideEnumeration(std::optional<Ds18b20DriverResult> r) {
    enumerationOverride_ = r;
}

void FakeDs18b20Bus::overrideStartConversion(
    std::optional<Ds18b20DriverResult> r) {
    startOverride_ = r;
}

Ds18b20DriverResult FakeDs18b20Bus::presence() {
    if (presenceOverride_.has_value()) return *presenceOverride_;
    return devices_.empty() ? Ds18b20DriverResult::NotFound
                            : Ds18b20DriverResult::Ok;
}

device_platform::Ds18b20Enumeration FakeDs18b20Bus::enumerate() {
    ++enumerations_;
    device_platform::Ds18b20Enumeration result;
    if (enumerationOverride_.has_value()) {
        result.result = *enumerationOverride_;
        return result;
    }
    result.result = Ds18b20DriverResult::Ok;
    for (const auto& entry : devices_) {
        if (result.count < device_platform::kMaxOneWireDevicesPerBus) {
            result.roms[result.count++] = entry.first;
        } else {
            result.overflow = true;
        }
    }
    return result;
}

Ds18b20DriverResult FakeDs18b20Bus::startConversionAll() {
    if (startOverride_.has_value()) return *startOverride_;
    ++conversions_;
    for (auto& entry : devices_) {
        entry.second.latched = entry.second.celsius;
        entry.second.freshSinceRead = true;
    }
    return Ds18b20DriverResult::Ok;
}

device_platform::Ds18b20ScratchpadRead FakeDs18b20Bus::read(OneWireRom rom) {
    ++reads_;
    device_platform::Ds18b20ScratchpadRead result;
    const auto found = devices_.find(rom);
    if (found == devices_.end()) {
        result.result = Ds18b20DriverResult::Timeout;
        return result;
    }
    Device& device = found->second;
    if (!device.freshSinceRead) ++staleReads_;
    device.freshSinceRead = false;
    result.result = device.readResult;
    if (result.result == Ds18b20DriverResult::Ok) {
        result.celsius = device.latched;
    }
    return result;
}

Ds18b20DriverResult FakeDs18b20Bus::setResolution12(OneWireRom rom) {
    (void)rom;
    ++resolutionCalls_;
    return setResolutionResult_;
}

}  // namespace device_platform_test_support
