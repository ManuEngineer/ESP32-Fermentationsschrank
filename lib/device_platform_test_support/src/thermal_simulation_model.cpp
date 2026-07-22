#include "thermal_simulation_model.hpp"

#include <algorithm>

namespace device_platform_test_support {

ThermalSimulationModel::ThermalSimulationModel(ThermalSimulationConfig config,
                                               double initialCelsius)
    : config_(config), celsius_(initialCelsius) {}

void ThermalSimulationModel::advance(uint64_t elapsedMs, bool heating,
                                     bool cooling) {
    const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;

    if (heating && !cooling) {
        celsius_ += config_.heatingRateCelsiusPerSecond * elapsedSeconds;
        return;
    }
    if (cooling && !heating) {
        celsius_ -= config_.coolingRateCelsiusPerSecond * elapsedSeconds;
        return;
    }
    if (heating && cooling) {
        return;
    }

    const double maxDrift =
        config_.idleDriftRateCelsiusPerSecond * elapsedSeconds;
    const double distanceToAmbient = config_.ambientCelsius - celsius_;
    const double drift = std::clamp(distanceToAmbient, -maxDrift, maxDrift);
    celsius_ += drift;
}

double ThermalSimulationModel::celsius() const { return celsius_; }

}  // namespace device_platform_test_support
