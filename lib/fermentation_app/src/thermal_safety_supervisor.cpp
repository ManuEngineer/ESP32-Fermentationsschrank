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
        case ThermalSafetyReason::TrendObservationPending:
            return "trend_observation_pending";
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
    trendBucketWidthMillis_ =
        (limits_.trendWindowMillis + kTrendBucketsPerWindow - 1U) /
        kTrendBucketsPerWindow;
    if (trendBucketWidthMillis_ == 0U) trendBucketWidthMillis_ = 1U;
    resetTrend();
    last_ = ThermalSafetyEvaluation{ThermalSafetyStatus::Blocked,
                                    ThermalSafetyReason::SampleMissing};
    return ThermalSafetyConfigureResult::Accepted;
}

void ThermalSafetySupervisor::clearConfiguration() noexcept {
    configured_ = false;
    limits_ = ThermalSafetyLimits{};
    invalidSinceMillis_.reset();
    resetTrend();
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

void ThermalSafetySupervisor::resetTrend() noexcept {
    for (auto& slot : trend_) slot = TrendSlot{};
    trendFirstMillis_.reset();
    trendLastMillis_.reset();
}

void ThermalSafetySupervisor::recordTrend(std::uint64_t timestampMillis,
                                          double celsius) noexcept {
    if (trendLastMillis_.has_value() && timestampMillis == *trendLastMillis_) {
        return;  // idempotent for an already recorded reading
    }
    // A reading older than the newest recorded one (time stepping backwards or
    // out-of-order arrival) cannot be placed in the history: fail-closed, the
    // observation restarts from this reading (no `Ok` until observed again).
    if (trendLastMillis_.has_value() && timestampMillis < *trendLastMillis_) {
        resetTrend();
    }
    // A gap longer than the sample-age limit breaks the observation: readings
    // before it can no longer prove what happened in between.
    if (trendLastMillis_.has_value() &&
        timestampMillis - *trendLastMillis_ > limits_.maxSampleAgeMillis) {
        resetTrend();
    }
    if (!trendFirstMillis_.has_value()) trendFirstMillis_ = timestampMillis;
    trendLastMillis_ = timestampMillis;
    const std::uint64_t bucket = timestampMillis / trendBucketWidthMillis_;
    auto& slot = trend_[bucket % kTrendSlots];
    if (slot.used && slot.bucketIndex == bucket)
        return;  // keep the first reading
    slot = TrendSlot{bucket, timestampMillis, celsius, true};
}

ThermalSafetySupervisor::TrendJudgement ThermalSafetySupervisor::judgeTrend(
    std::uint64_t newestMillis, double newestCelsius) const noexcept {
    // Observed continuously for at least half the window? Otherwise the trend
    // cannot be judged and no `Ok` may be given.
    const std::uint64_t half = limits_.trendWindowMillis / 2U;
    if (!trendFirstMillis_.has_value() ||
        newestMillis - *trendFirstMillis_ < half) {
        return TrendJudgement::Pending;
    }
    // Worst rise over every baseline between half a window and one window old.
    // `Within` needs at least one such baseline: without a comparison reading
    // inside the prescribed window (e.g. samples further apart than the
    // window) nothing was judged and no `Ok` may result.
    bool exceeded = false;
    bool baselineFound = false;
    for (const auto& slot : trend_) {
        if (!slot.used || slot.timestampMillis >= newestMillis) continue;
        const std::uint64_t span = newestMillis - slot.timestampMillis;
        if (span < half || span > limits_.trendWindowMillis) continue;
        baselineFound = true;
        const double minutes = static_cast<double>(span) / 60'000.0;
        if ((newestCelsius - slot.celsius) / minutes >
            limits_.coolingMaxRiseCelsiusPerMinute) {
            exceeded = true;
        }
    }
    if (!baselineFound) return TrendJudgement::Pending;
    return exceeded ? TrendJudgement::Exceeded : TrendJudgement::Within;
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
    switch (judgeTrend(cooling.timestampMillis, cooling.celsius)) {
        case TrendJudgement::Exceeded:
            return trip(ThermalSafetyReason::CoolingRiseRate);
        case TrendJudgement::Pending:
            last_ = ThermalSafetyEvaluation{
                ThermalSafetyStatus::Blocked,
                ThermalSafetyReason::TrendObservationPending};
            return last_;
        case TrendJudgement::Within:
            break;
    }

    last_ = ThermalSafetyEvaluation{ThermalSafetyStatus::Ok,
                                    ThermalSafetyReason::None};
    return last_;
}

}  // namespace fermentation
