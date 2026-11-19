#include "control_sensor_evidence.hpp"

namespace fermentation {

ControlSensorEvidenceProducer::ControlSensorEvidenceProducer(
    const device_platform::ITemperatureSource& air,
    const device_platform::ITemperatureSource& cooling,
    const device_platform::ITemperatureSource& product,
    const device_platform::ITimeSource& time,
    const std::optional<device_platform::SensorQualityConfig>& config)
    : airSource_(air),
      coolingSource_(cooling),
      productSource_(product),
      time_(time) {
    if (config.has_value()) {
        air_.emplace(*config);
        cooling_.emplace(*config);
        product_.emplace(*config);
    }
}

std::optional<CrossRolePlausibilityContext>
ControlSensorEvidenceProducer::sample(ProcessState phase) {
    if (!configured()) {
        return std::nullopt;
    }
    const std::uint64_t now = time_.monotonicMillis();
    static_cast<void>(air_->ingest(airSource_.read(), now));
    static_cast<void>(cooling_->ingest(coolingSource_.read(), now));
    static_cast<void>(product_->ingest(productSource_.read(), now));

    CrossRolePlausibilityContext context;
    context.phase = phase;
    context.evaluationMonotonicMillis = now;
    context.air = air_->snapshot(now);
    context.cooling = cooling_->snapshot(now);
    context.product = product_->snapshot(now);
    return context;
}

}  // namespace fermentation
