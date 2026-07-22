#include "mock_temperature_source.hpp"

namespace device_platform {

MockTemperatureSource::MockTemperatureSource(double initialCelsius)
    : celsius_(initialCelsius) {}

TemperatureReading MockTemperatureSource::read() const {
    return TemperatureReading{available_, celsius_};
}

void MockTemperatureSource::setCelsius(double celsius) { celsius_ = celsius; }

void MockTemperatureSource::setAvailable(bool available) {
    available_ = available;
}

}  // namespace device_platform
