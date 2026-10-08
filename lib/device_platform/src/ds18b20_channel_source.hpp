#pragma once

#include "ds18b20_sampling_engine.hpp"
#include "temperature_source.hpp"

namespace device_platform {

// ITemperatureSource-Sicht auf einen technischen Kanal der Sampling-Engine.
// Die Rollen ordnet erst die Anwendung zu (ADR-013, Plan Abschnitt 4c).
class Ds18b20ChannelSource final : public ITemperatureSource {
   public:
    Ds18b20ChannelSource(const Ds18b20SamplingEngine& engine, uint8_t channel)
        : engine_(engine), channel_(channel) {}

    [[nodiscard]] TemperatureReading read() const override {
        return engine_.reading(channel_);
    }

   private:
    const Ds18b20SamplingEngine& engine_;
    uint8_t channel_;
};

}  // namespace device_platform
