#include "control_sensor_evidence.hpp"

namespace fermentation {

ControlSensorEvidenceProducer::ControlSensorEvidenceProducer(
    const device_platform::ITemperatureSource& air,
    const device_platform::ITemperatureSource& cooling,
    const device_platform::ITemperatureSource& product,
    const device_platform::ITimeSource& time,
    const std::optional<device_platform::SensorQualityConfig>& config)
    : ControlSensorEvidenceProducer(
          air, cooling, product, time,
          ControlSensorQualityConfigs{config, config, config}) {}

ControlSensorEvidenceProducer::ControlSensorEvidenceProducer(
    const device_platform::ITemperatureSource& air,
    const device_platform::ITemperatureSource& cooling,
    const device_platform::ITemperatureSource& product,
    const device_platform::ITimeSource& time,
    const ControlSensorQualityConfigs& configs)
    : airSource_(air),
      coolingSource_(cooling),
      productSource_(product),
      time_(time) {
    if (configs.air.has_value()) air_.emplace(*configs.air);
    if (configs.cooling.has_value()) cooling_.emplace(*configs.cooling);
    if (configs.product.has_value()) product_.emplace(*configs.product);
}

std::optional<CrossRolePlausibilityContext>
ControlSensorEvidenceProducer::sample(ProcessState phase) {
    if (!configured()) {
        return std::nullopt;
    }
    const std::uint64_t now = time_.monotonicMillis();
    static_cast<void>(air_->ingest(airSource_.read(), now));
    static_cast<void>(cooling_->ingest(coolingSource_.read(), now));
    if (product_.has_value()) {
        static_cast<void>(product_->ingest(productSource_.read(), now));
    }

    CrossRolePlausibilityContext context;
    context.phase = phase;
    context.evaluationMonotonicMillis = now;
    context.air = air_->snapshot(now);
    context.cooling = cooling_->snapshot(now);
    // A role without a configuration keeps its default snapshot (not Valid).
    if (product_.has_value()) context.product = product_->snapshot(now);
    return context;
}

RawTemperatureSamples ControlSensorEvidenceProducer::readRawSamples() const {
    RawTemperatureSamples samples;
    samples.air = airSource_.read();
    samples.product = productSource_.read();
    samples.cooling = coolingSource_.read();
    return samples;
}

}  // namespace fermentation
