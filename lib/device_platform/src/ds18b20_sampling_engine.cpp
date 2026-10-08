#include "ds18b20_sampling_engine.hpp"

#include <algorithm>

namespace device_platform {
namespace {

constexpr std::size_t kExpectedRomBus = 0U;
constexpr std::size_t kSingleDeviceBus = 1U;

TemperatureSampleStatus statusFor(Ds18b20DriverResult result) {
    switch (result) {
        case Ds18b20DriverResult::Ok:
            return TemperatureSampleStatus::Ok;
        case Ds18b20DriverResult::NotFound:
            return TemperatureSampleStatus::MissingSample;
        case Ds18b20DriverResult::Timeout:
            return TemperatureSampleStatus::BusFault;
        case Ds18b20DriverResult::InvalidCrc:
            return TemperatureSampleStatus::CrcFault;
        case Ds18b20DriverResult::PowerOnValue:
            return TemperatureSampleStatus::KnownInvalidMeasurement;
        case Ds18b20DriverResult::Other:
            return TemperatureSampleStatus::BusFault;
    }
    return TemperatureSampleStatus::BusFault;
}

std::optional<SensorIdentity> identityOf(std::optional<OneWireRom> rom) {
    if (!rom.has_value()) return std::nullopt;
    return SensorIdentity::create(*rom).identity;
}

TemperatureReading makeReading(std::optional<OneWireRom> rom,
                               TemperatureSampleStatus status,
                               std::optional<double> celsius,
                               uint64_t timestampMs) {
    // Konsistent by construction: celsius gesetzt genau bei Ok.
    return TemperatureReading::create(identityOf(rom), timestampMs, status,
                                      celsius)
        .reading.value();
}

bool contains(const Ds18b20Enumeration& enumeration, OneWireRom rom) {
    for (uint8_t i = 0U; i < enumeration.count; ++i) {
        if (enumeration.roms[i] == rom) return true;
    }
    return false;
}

bool hasZeroRom(const Ds18b20Enumeration& enumeration) {
    for (uint8_t i = 0U; i < enumeration.count; ++i) {
        if (enumeration.roms[i] == 0U) return true;
    }
    return false;
}

}  // namespace

Ds18b20SamplingEngine::Ds18b20SamplingEngine(const ITimeSource& time,
                                             IDs18b20Bus& expectedRomBus,
                                             IDs18b20Bus& singleDeviceBus)
    : time_(time), buses_{&expectedRomBus, &singleDeviceBus} {
    for (auto& slot : readings_) {
        slot = makeReading(std::nullopt, TemperatureSampleStatus::MissingSample,
                           std::nullopt, 0U);
    }
}

void Ds18b20SamplingEngine::setBinding(
    const std::array<std::optional<Ds18b20ChannelBinding>,
                     kExpectedRomChannelCount>& bindings) {
    std::array<std::optional<OneWireRom>, kExpectedRomChannelCount> candidate{};
    bool valid = true;
    for (const auto& entry : bindings) {
        if (!entry.has_value() || entry->channel >= kExpectedRomChannelCount ||
            entry->expectedRom == 0U || candidate[entry->channel].has_value()) {
            valid = false;
            break;
        }
        candidate[entry->channel] = entry->expectedRom;
    }
    if (valid) {
        valid = candidate[0].has_value() && candidate[1].has_value() &&
                *candidate[0] != *candidate[1];
    }
    std::lock_guard<std::mutex> lock(mutex_);
    bindingValid_ = valid;
    binding_ =
        valid
            ? candidate
            : std::array<std::optional<OneWireRom>, kExpectedRomChannelCount>{};
    for (auto& bus : resolutionSet_) bus.fill(0U);
    report_.bindingValid = valid;
}

std::optional<OneWireRom> Ds18b20SamplingEngine::expectedRomOf(
    uint8_t channel) const {
    if (!bindingValid_ || channel >= kExpectedRomChannelCount) {
        return std::nullopt;
    }
    return binding_[channel];
}

bool Ds18b20SamplingEngine::resolutionKnown(std::size_t bus,
                                            OneWireRom rom) const {
    const auto& set = resolutionSet_[bus];
    return std::find(set.begin(), set.end(), rom) != set.end();
}

void Ds18b20SamplingEngine::markResolution(std::size_t bus, OneWireRom rom) {
    auto& set = resolutionSet_[bus];
    const auto empty = std::find(set.begin(), set.end(), OneWireRom{0U});
    if (empty != set.end()) *empty = rom;
}

void Ds18b20SamplingEngine::forgetResolution(std::size_t bus) {
    resolutionSet_[bus].fill(0U);
}

void Ds18b20SamplingEngine::pruneResolution(
    std::size_t bus, const Ds18b20Enumeration& enumeration) {
    for (auto& rom : resolutionSet_[bus]) {
        if (rom != 0U && !contains(enumeration, rom)) rom = 0U;
    }
}

void Ds18b20SamplingEngine::publish(uint8_t channel,
                                    std::optional<OneWireRom> identity,
                                    TemperatureSampleStatus status,
                                    std::optional<double> celsius,
                                    uint64_t timestampMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    readings_[channel] = makeReading(identity, status, celsius, timestampMs);
}

void Ds18b20SamplingEngine::publishFault(uint8_t channel,
                                         std::optional<OneWireRom> identity,
                                         Ds18b20DriverResult result,
                                         uint64_t timestampMs) {
    publish(channel, identity, statusFor(result), std::nullopt, timestampMs);
}

void Ds18b20SamplingEngine::planExpectedRomBus(uint64_t now) {
    IDs18b20Bus& bus = *buses_[kExpectedRomBus];
    Ds18b20BusReport& report = workingReport_.expectedRomBus;
    report = Ds18b20BusReport{};
    report.presence = bus.presence();

    const std::optional<OneWireRom> id0 = expectedRomOf(0U);
    const std::optional<OneWireRom> id1 = expectedRomOf(1U);
    const auto faultBoth = [&](Ds18b20DriverResult result) {
        publishFault(0U, id0, result, now);
        publishFault(1U, id1, result, now);
    };

    if (report.presence == Ds18b20DriverResult::Ok) {
        report.enumeration = bus.enumerate();
        if (report.enumeration.result == Ds18b20DriverResult::Ok &&
            hasZeroRom(report.enumeration)) {
            report.enumeration.result = Ds18b20DriverResult::Other;
        }
    }

    if (!bindingValid_) {
        // 4a: ohne verifizierte Bindung nie Ok, keine Identitaet.
        forgetResolution(kExpectedRomBus);
        publish(0U, std::nullopt, TemperatureSampleStatus::MissingSample,
                std::nullopt, now);
        publish(1U, std::nullopt, TemperatureSampleStatus::MissingSample,
                std::nullopt, now);
        return;
    }
    if (report.presence != Ds18b20DriverResult::Ok) {
        forgetResolution(kExpectedRomBus);
        faultBoth(report.presence);
        return;
    }
    if (report.enumeration.result != Ds18b20DriverResult::Ok) {
        forgetResolution(kExpectedRomBus);
        faultBoth(report.enumeration.result);
        return;
    }
    const Ds18b20Enumeration& found = report.enumeration;
    pruneResolution(kExpectedRomBus, found);

    bool conflict = found.overflow;
    for (uint8_t i = 0U; i < found.count; ++i) {
        if (found.roms[i] != *id0 && found.roms[i] != *id1) conflict = true;
    }
    report.bindingConflict = conflict;
    if (conflict) {
        // Unbekanntes zusaetzliches ROM: beide Kanaele fail-closed.
        publish(0U, id0, TemperatureSampleStatus::MissingSample, std::nullopt,
                now);
        publish(1U, id1, TemperatureSampleStatus::MissingSample, std::nullopt,
                now);
        return;
    }

    for (uint8_t channel = 0U; channel < kExpectedRomChannelCount; ++channel) {
        const OneWireRom rom = *expectedRomOf(channel);
        if (!contains(found, rom)) {
            publish(channel, rom, TemperatureSampleStatus::MissingSample,
                    std::nullopt, now);
            continue;
        }
        if (!resolutionKnown(kExpectedRomBus, rom)) {
            const Ds18b20DriverResult set = bus.setResolution12(rom);
            if (set != Ds18b20DriverResult::Ok) {
                publishFault(channel, rom, set, now);
                continue;
            }
            markResolution(kExpectedRomBus, rom);
        }
        plan_[channel] = ChannelPlan{true, rom};
    }
    if (plan_[0].convert || plan_[1].convert) {
        const Ds18b20DriverResult started = bus.startConversionAll();
        if (started != Ds18b20DriverResult::Ok) {
            forgetResolution(kExpectedRomBus);
            for (uint8_t channel = 0U; channel < kExpectedRomChannelCount;
                 ++channel) {
                if (plan_[channel].convert) {
                    publishFault(channel, plan_[channel].rom, started, now);
                    plan_[channel] = ChannelPlan{};
                }
            }
            return;
        }
        busTriggered_[kExpectedRomBus] = true;
    }
}

void Ds18b20SamplingEngine::planSingleDeviceBus(uint64_t now) {
    IDs18b20Bus& bus = *buses_[kSingleDeviceBus];
    Ds18b20BusReport& report = workingReport_.singleDeviceBus;
    report = Ds18b20BusReport{};
    report.presence = bus.presence();
    constexpr uint8_t channel = kSingleDeviceChannel;
    // 4b: Nicht-Ok-Proben tragen das zuletzt gesehene einzelne ROM.
    const auto fault = [&](Ds18b20DriverResult result) {
        forgetResolution(kSingleDeviceBus);
        publishFault(channel, singleDeviceLastSeen_, result, now);
    };

    if (report.presence != Ds18b20DriverResult::Ok) {
        fault(report.presence);
        return;
    }
    report.enumeration = bus.enumerate();
    const Ds18b20Enumeration& found = report.enumeration;
    if (found.result != Ds18b20DriverResult::Ok) {
        fault(found.result);
        return;
    }
    if (found.overflow || found.count >= 2U || hasZeroRom(found)) {
        // Zwei oder mehr Geraete: Fehlanschluss, keine Auswahl nach
        // Reihenfolge.
        fault(Ds18b20DriverResult::Other);
        return;
    }
    if (found.count == 0U) {
        fault(Ds18b20DriverResult::NotFound);
        return;
    }
    const OneWireRom rom = found.roms[0];
    singleDeviceLastSeen_ = rom;
    pruneResolution(kSingleDeviceBus, found);
    if (!resolutionKnown(kSingleDeviceBus, rom)) {
        const Ds18b20DriverResult set = bus.setResolution12(rom);
        if (set != Ds18b20DriverResult::Ok) {
            publishFault(channel, rom, set, now);
            return;
        }
        markResolution(kSingleDeviceBus, rom);
    }
    const Ds18b20DriverResult started = bus.startConversionAll();
    if (started != Ds18b20DriverResult::Ok) {
        forgetResolution(kSingleDeviceBus);
        publishFault(channel, rom, started, now);
        return;
    }
    plan_[channel] = ChannelPlan{true, rom};
    busTriggered_[kSingleDeviceBus] = true;
}

void Ds18b20SamplingEngine::beginCycle(uint64_t now) {
    cycleStartMs_ = now;
    nextCycleStartMs_ = now + kCyclePeriodMs;
    plan_ = {};
    busTriggered_ = {};
    workingReport_ = Ds18b20EnumerationReport{};
    planExpectedRomBus(now);
    planSingleDeviceBus(now);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        workingReport_.bindingValid = bindingValid_;
        workingReport_.singleDeviceLastSeen = singleDeviceLastSeen_;
        report_ = workingReport_;
    }
    if (busTriggered_[kExpectedRomBus] || busTriggered_[kSingleDeviceBus]) {
        conversionStartedMs_ = time_.monotonicMillis();
        phase_ = Phase::WaitingForConversion;
        return;
    }
    ++completedCycles_;
}

void Ds18b20SamplingEngine::finishCycle() {
    for (uint8_t channel = 0U; channel < kChannelCount; ++channel) {
        if (!plan_[channel].convert) continue;
        const std::size_t busIndex = channel < kExpectedRomChannelCount
                                         ? kExpectedRomBus
                                         : kSingleDeviceBus;
        const Ds18b20ScratchpadRead read =
            buses_[busIndex]->read(plan_[channel].rom);
        const uint64_t timestamp = time_.monotonicMillis();
        if (read.result == Ds18b20DriverResult::Ok &&
            read.celsius.has_value()) {
            publish(channel, plan_[channel].rom, TemperatureSampleStatus::Ok,
                    read.celsius, timestamp);
        } else {
            const Ds18b20DriverResult result =
                read.result == Ds18b20DriverResult::Ok
                    ? Ds18b20DriverResult::Other
                    : read.result;
            forgetResolution(busIndex);
            publishFault(channel, plan_[channel].rom, result, timestamp);
        }
    }
    const uint64_t now = time_.monotonicMillis();
    if (now > nextCycleStartMs_) nextCycleStartMs_ = now;
    phase_ = Phase::Idle;
    ++completedCycles_;
}

void Ds18b20SamplingEngine::step() {
    const uint64_t now = time_.monotonicMillis();
    if (phase_ == Phase::Idle) {
        if (now >= nextCycleStartMs_) beginCycle(now);
    } else if (now >= conversionStartedMs_ + kConversionWaitMs) {
        finishCycle();
    }
}

uint64_t Ds18b20SamplingEngine::nextDueMillis() const {
    return phase_ == Phase::Idle ? nextCycleStartMs_
                                 : conversionStartedMs_ + kConversionWaitMs;
}

uint64_t Ds18b20SamplingEngine::completedCycles() const {
    return completedCycles_;
}

TemperatureReading Ds18b20SamplingEngine::reading(uint8_t channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (channel >= kChannelCount) {
        return makeReading(std::nullopt, TemperatureSampleStatus::MissingSample,
                           std::nullopt, 0U);
    }
    return *readings_[channel];
}

Ds18b20EnumerationReport Ds18b20SamplingEngine::report() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return report_;
}

}  // namespace device_platform
