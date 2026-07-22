#include "mock_temperature_source.hpp"

namespace device_platform_test_support {

MockTemperatureSource::MockTemperatureSource(double initialCelsius)
    : celsius_(initialCelsius) {}

device_platform::TemperatureReading MockTemperatureSource::read() const {
    return device_platform::TemperatureReading{available_, celsius_};
}

void MockTemperatureSource::setCelsius(double celsius) { celsius_ = celsius; }

void MockTemperatureSource::setAvailable(bool available) {
    available_ = available;
}

}  // namespace device_platform_test_support
