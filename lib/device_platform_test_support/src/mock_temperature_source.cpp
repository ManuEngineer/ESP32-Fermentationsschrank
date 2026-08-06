#include "mock_temperature_source.hpp"

namespace device_platform_test_support {

namespace {

device_platform::TemperatureReading makeOkReading(
    std::optional<device_platform::SensorIdentity> identity,
    uint64_t monotonicTimestampMs, double celsius) {
    return device_platform::TemperatureReading::create(
               identity, monotonicTimestampMs,
               device_platform::TemperatureSampleStatus::Ok, celsius)
        .reading.value();
}

}  // namespace

MockTemperatureSource::MockTemperatureSource(
    std::optional<device_platform::SensorIdentity> identity,
    uint64_t monotonicTimestampMs, double celsius)
    : reading_(makeOkReading(identity, monotonicTimestampMs, celsius)) {}

device_platform::TemperatureReading MockTemperatureSource::read() const {
    return reading_;
}

void MockTemperatureSource::setReading(
    std::optional<device_platform::SensorIdentity> identity,
    uint64_t monotonicTimestampMs, double celsius) {
    reading_ = makeOkReading(identity, monotonicTimestampMs, celsius);
}

void MockTemperatureSource::setFault(
    std::optional<device_platform::SensorIdentity> identity,
    uint64_t monotonicTimestampMs,
    device_platform::TemperatureSampleStatus status) {
    reading_ = device_platform::TemperatureReading::create(
                   identity, monotonicTimestampMs, status, std::nullopt)
                   .reading.value();
}

}  // namespace device_platform_test_support
