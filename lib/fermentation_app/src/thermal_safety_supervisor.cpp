#include "thermal_safety_supervisor.hpp"

#include <cmath>

#include "sensor_limits.hpp"

namespace fermentation {

namespace {

bool finite(double value) noexcept { return std::isfinite(value); }

bool inMeasuringRange(double value) noexcept {
    return finite(value) &&
           value >= device_platform::sensor_limits::kAbsoluteMinCelsius &&
           value <= device_platform::sensor_limits::kAbsoluteMaxCelsius;
}

// Strictly ordered: emergencyLower < interventionLower < interventionUpper <
// emergencyUpper, all inside the sensor measuring range.
bool orderedBand(double emergencyLower, double interventionLower,
                 double interventionUpper, double emergencyUpper) noexcept {
    return inMeasuringRange(emergencyLower) &&
           inMeasuringRange(interventionLower) &&
           inMeasuringRange(interventionUpper) &&
           inMeasuringRange(emergencyUpper) &&
           emergencyLower < interventionLower &&
           interventionLower < interventionUpper &&
           interventionUpper < emergencyUpper;
}

enum class Validity : std::uint8_t { Valid, Missing, Fault, Stale, OutOfRange };

struct Checked {
    Validity validity{Validity::Missing};
    double celsius{0.0};
    std::uint64_t timestampMillis{0U};
};

Checked check(const std::optional<device_platform::TemperatureReading>& reading,
              std::uint64_t now, std::uint64_t maxAgeMillis) noexcept {
    Checked result;
    if (!reading.has_value()) return result;
    if (reading->status() != device_platform::TemperatureSampleStatus::Ok ||
        !reading->celsius().has_value()) {
        result.validity = Validity::Fault;
        return result;
    }
    const auto timestamp = reading->monotonicTimestampMs();
    if (timestamp > now || now - timestamp > maxAgeMillis) {
        result.validity = Validity::Stale;
        return result;
    }
    if (!inMeasuringRange(*reading->celsius())) {
        result.validity = Validity::OutOfRange;
        return result;
    }
    result.validity = Validity::Valid;
    result.celsius = *reading->celsius();
    result.timestampMillis = timestamp;
    return result;
}

ThermalSafetyReason invalidReason(Validity validity) noexcept {
    switch (validity) {
        case Validity::Missing:
            return ThermalSafetyReason::SampleMissing;
        case Validity::Fault:
            return ThermalSafetyReason::SampleFault;
        case Validity::Stale:
            return ThermalSafetyReason::SampleStale;
        case Validity::OutOfRange:
            return ThermalSafetyReason::OutOfMeasuringRange;
        case Validity::Valid:
            break;
    }
    return ThermalSafetyReason::SampleMissing;
}

}  // namespace

const char* thermalSafetyReasonName(ThermalSafetyReason reason) noexcept {
    switch (reason) {
        case ThermalSafetyReason::None:
            return "none";
        case ThermalSafetyReason::NotConfigured:
            return "not_configured";
        case ThermalSafetyReason::SampleMissing:
            return "sample_missing";
        case ThermalSafetyReason::SampleFault:
            return "sample_fault";
        case ThermalSafetyReason::SampleStale:
            return "sample_stale";
        case ThermalSafetyReason::OutOfMeasuringRange:
            return "out_of_measuring_range";
        case ThermalSafetyReason::SampleLoss:
            return "sample_loss";
        case ThermalSafetyReason::AirLowerEmergency:
            return "air_lower_emergency";
        case ThermalSafetyReason::AirLowerIntervention:
            return "air_lower_intervention";
        case ThermalSafetyReason::AirUpperIntervention:
            return "air_upper_intervention";
        case ThermalSafetyReason::AirUpperEmergency:
            return "air_upper_emergency";
        case ThermalSafetyReason::CoolingLowerEmergency:
            return "cooling_lower_emergency";
        case ThermalSafetyReason::CoolingLowerIntervention:
            return "cooling_lower_intervention";
        case ThermalSafetyReason::CoolingUpperIntervention:
            return "cooling_upper_intervention";
        case ThermalSafetyReason::CoolingUpperEmergency:
            return "cooling_upper_emergency";
        case ThermalSafetyReason::CoolingRiseRate:
            return "cooling_rise_rate";
    }
    return "unknown";
}

const char* thermalSafetyStatusName(ThermalSafetyStatus status) noexcept {
    switch (status) {
        case ThermalSafetyStatus::Unconfigured:
            return "unconfigured";
        case ThermalSafetyStatus::Ok:
            return "ok";
        case ThermalSafetyStatus::Blocked:
            return "blocked";
        case ThermalSafetyStatus::Tripped:
            return "tripped";
    }
    return "unknown";
}

bool validateThermalSafetyLimits(const ThermalSafetyLimits& limits) noexcept {
    return orderedBand(limits.airEmergencyLowerCelsius,
                       limits.airInterventionLowerCelsius,
                       limits.airInterventionUpperCelsius,
                       limits.airEmergencyUpperCelsius) &&
           orderedBand(limits.coolingEmergencyLowerCelsius,
                       limits.coolingInterventionLowerCelsius,
                       limits.coolingInterventionUpperCelsius,
                       limits.coolingEmergencyUpperCelsius) &&
           finite(limits.coolingMaxRiseCelsiusPerMinute) &&
           limits.coolingMaxRiseCelsiusPerMinute > 0.0 &&
           limits.coolingMaxRiseCelsiusPerMinute <=
               kThermalSafetyMaxRiseCelsiusPerMinute &&
           limits.trendWindowMillis >= kThermalSafetyMinTrendWindowMillis &&
           limits.trendWindowMillis <= kThermalSafetyMaxTrendWindowMillis &&
           limits.maxSampleAgeMillis >= kThermalSafetyMinSampleAgeMillis &&
           limits.maxSampleAgeMillis <= kThermalSafetyMaxSampleAgeMillis &&
           limits.invalidLatchMillis >= limits.maxSampleAgeMillis &&
           limits.invalidLatchMillis <= kThermalSafetyMaxInvalidLatchMillis;
}

ThermalSafetyConfigureResult ThermalSafetySupervisor::configure(
    const ThermalSafetyLimits& limits) noexcept {
    if (latched_.has_value()) return ThermalSafetyConfigureResult::Latched;
    if (!validateThermalSafetyLimits(limits)) {
        return ThermalSafetyConfigureResult::InvalidLimits;
    }
    limits_ = limits;
    configured_ = true;
    invalidSinceMillis_.reset();
    trendCount_ = 0U;
    last_ = ThermalSafetyEvaluation{ThermalSafetyStatus::Blocked,
                                    ThermalSafetyReason::SampleMissing};
    return ThermalSafetyConfigureResult::Accepted;
}

void ThermalSafetySupervisor::clearConfiguration() noexcept {
    configured_ = false;
    limits_ = ThermalSafetyLimits{};
    invalidSinceMillis_.reset();
    trendCount_ = 0U;
    if (!latched_.has_value()) {
        last_ = ThermalSafetyEvaluation{};
    }
}

ThermalSafetyEvaluation ThermalSafetySupervisor::trip(
    ThermalSafetyReason reason) noexcept {
    if (!latched_.has_value()) latched_ = reason;
    last_ = ThermalSafetyEvaluation{ThermalSafetyStatus::Tripped, *latched_};
    return last_;
}

void ThermalSafetySupervisor::recordTrend(std::uint64_t timestampMillis,
                                          double celsius) noexcept {
    if (trendCount_ > 0U &&
        timestampMillis <= trend_[trendCount_ - 1U].timestampMillis) {
        return;
    }
    // Drop points that left the window relative to the newest one.
    std::size_t first = 0U;
    while (first < trendCount_ &&
           timestampMillis - trend_[first].timestampMillis >
               limits_.trendWindowMillis) {
        ++first;
    }
    if (first > 0U) {
        for (std::size_t i = first; i < trendCount_; ++i) {
            trend_[i - first] = trend_[i];
        }
        trendCount_ -= first;
    }
    if (trendCount_ == kTrendCapacity) {
        for (std::size_t i = 1U; i < trendCount_; ++i)
            trend_[i - 1U] = trend_[i];
        --trendCount_;
    }
    trend_[trendCount_++] = TrendPoint{timestampMillis, celsius};
}

bool ThermalSafetySupervisor::trendExceeded() const noexcept {
    if (trendCount_ < 2U) return false;
    const auto& oldest = trend_[0];
    const auto& newest = trend_[trendCount_ - 1U];
    const auto span = newest.timestampMillis - oldest.timestampMillis;
    // A rate needs a meaningful span; otherwise one noisy step would trip.
    if (span < limits_.trendWindowMillis / 2U) return false;
    const double minutes = static_cast<double>(span) / 60'000.0;
    return (newest.celsius - oldest.celsius) / minutes >
           limits_.coolingMaxRiseCelsiusPerMinute;
}

ThermalSafetyEvaluation ThermalSafetySupervisor::evaluate(
    const RawTemperatureSamples& samples, std::uint64_t nowMonotonicMillis) {
    if (latched_.has_value()) return trip(*latched_);
    if (!configured_) {
        last_ = ThermalSafetyEvaluation{};
        return last_;
    }
    const auto air =
        check(samples.air, nowMonotonicMillis, limits_.maxSampleAgeMillis);
    const auto cooling =
        check(samples.cooling, nowMonotonicMillis, limits_.maxSampleAgeMillis);

    // Limits on every valid reading: an excursion latches immediately, even
    // when the other sensor is invalid.
    if (cooling.validity == Validity::Valid) {
        if (cooling.celsius <= limits_.coolingEmergencyLowerCelsius)
            return trip(ThermalSafetyReason::CoolingLowerEmergency);
        if (cooling.celsius >= limits_.coolingEmergencyUpperCelsius)
            return trip(ThermalSafetyReason::CoolingUpperEmergency);
        if (cooling.celsius <= limits_.coolingInterventionLowerCelsius)
            return trip(ThermalSafetyReason::CoolingLowerIntervention);
        if (cooling.celsius >= limits_.coolingInterventionUpperCelsius)
            return trip(ThermalSafetyReason::CoolingUpperIntervention);
    }
    if (air.validity == Validity::Valid) {
        if (air.celsius <= limits_.airEmergencyLowerCelsius)
            return trip(ThermalSafetyReason::AirLowerEmergency);
        if (air.celsius >= limits_.airEmergencyUpperCelsius)
            return trip(ThermalSafetyReason::AirUpperEmergency);
        if (air.celsius <= limits_.airInterventionLowerCelsius)
            return trip(ThermalSafetyReason::AirLowerIntervention);
        if (air.celsius >= limits_.airInterventionUpperCelsius)
            return trip(ThermalSafetyReason::AirUpperIntervention);
    }

    // Both roles must be valid for any Peltier permission.
    if (air.validity != Validity::Valid ||
        cooling.validity != Validity::Valid) {
        if (!invalidSinceMillis_.has_value() ||
            nowMonotonicMillis < *invalidSinceMillis_) {
            // A time source stepping backwards restarts the episode instead of
            // underflowing into an immediate latch.
            invalidSinceMillis_ = nowMonotonicMillis;
        }
        if (nowMonotonicMillis - *invalidSinceMillis_ >=
            limits_.invalidLatchMillis) {
            return trip(ThermalSafetyReason::SampleLoss);
        }
        last_ = ThermalSafetyEvaluation{
            ThermalSafetyStatus::Blocked,
            invalidReason(air.validity != Validity::Valid ? air.validity
                                                          : cooling.validity)};
        return last_;
    }
    invalidSinceMillis_.reset();

    recordTrend(cooling.timestampMillis, cooling.celsius);
    if (trendExceeded()) return trip(ThermalSafetyReason::CoolingRiseRate);

    last_ = ThermalSafetyEvaluation{ThermalSafetyStatus::Ok,
                                    ThermalSafetyReason::None};
    return last_;
}

}  // namespace fermentation
