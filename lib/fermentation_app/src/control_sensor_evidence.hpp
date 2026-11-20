#pragma once

#include <optional>

#include "process_state_machine.hpp"
#include "raw_temperature_samples.hpp"
#include "sensor_quality_config.hpp"
#include "sensor_quality_pipeline.hpp"
#include "sensor_selection.hpp"
#include "temperature_source.hpp"
#include "time_source.hpp"

namespace fermentation {

// Composes the existing per-role SensorQualityPipeline instances (air, product,
// cooling body) from the three abstract temperature sources (#218). It adds
// no filter, plausibility or state logic of its own. Without a quality
// configuration (the product until #35 releases real values) it produces no
// evidence at all, so no sensor can ever become Valid.
class ControlSensorEvidenceProducer {
   public:
    ControlSensorEvidenceProducer(
        const device_platform::ITemperatureSource& air,
        const device_platform::ITemperatureSource& cooling,
        const device_platform::ITemperatureSource& product,
        const device_platform::ITimeSource& time,
        const std::optional<device_platform::SensorQualityConfig>& config);

    [[nodiscard]] bool configured() const noexcept { return air_.has_value(); }

    // Ingests the current reading of every role and returns the evidence to
    // hand to FermentationApplication::publishOwningRuntimeEvidence().
    // nullopt when unconfigured.
    [[nodiscard]] std::optional<CrossRolePlausibilityContext> sample(
        ProcessState phase);

    // Current technical raw readings of the same three sources (no quality
    // judgement; used for the unqualified local display and for the firmware
    // temperature safety supervisor). Works without a quality configuration.
    [[nodiscard]] RawTemperatureSamples readRawSamples() const;

   private:
    const device_platform::ITemperatureSource& airSource_;
    const device_platform::ITemperatureSource& coolingSource_;
    const device_platform::ITemperatureSource& productSource_;
    const device_platform::ITimeSource& time_;
    std::optional<device_platform::SensorQualityPipeline> air_;
    std::optional<device_platform::SensorQualityPipeline> cooling_;
    std::optional<device_platform::SensorQualityPipeline> product_;
};

}  // namespace fermentation
