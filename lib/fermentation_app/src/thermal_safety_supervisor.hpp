#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "raw_temperature_samples.hpp"

namespace fermentation {

// Firmware temperature safety for the Peltier path (R1 trial plan, S6; owner
// decision D5). It evaluates the RAW readings of the cooling-body and the
// cabinet-air sensor, independent of the SensorQualityConfig / filter chain,
// and is consumed by the existing ActuationInterlock as one more evidence
// field. It is NOT hardware-independent protection: an MCU hang, a driver
// defect or a missing sensor poll defeats it. The independent over-current and
// one-time temperature fuse of `docs/HARDWARE.md` remain separate obligations.
//
//   Unconfigured : no limits set (product default, dead end for the Peltier).
//   Ok           : fresh valid raw readings, all limits and the trend
//   respected. Blocked      : readings missing/stale/faulty/out of measuring
//   range; the
//                  Peltier is blocked while this lasts (not latched).
//   Tripped      : latched (volatile; re-arm = restart + limits set again):
//                  limit exceeded, trend exceeded or readings invalid for too
//                  long. No automatic counter-direction attempt.
enum class ThermalSafetyStatus : std::uint8_t {
    Unconfigured,
    Ok,
    Blocked,
    Tripped,
};

enum class ThermalSafetyReason : std::uint8_t {
    None,
    NotConfigured,
    SampleMissing,
    SampleFault,
    SampleStale,
    OutOfMeasuringRange,
    SampleLoss,
    AirLowerEmergency,
    AirLowerIntervention,
    AirUpperIntervention,
    AirUpperEmergency,
    CoolingLowerEmergency,
    CoolingLowerIntervention,
    CoolingUpperIntervention,
    CoolingUpperEmergency,
    CoolingRiseRate,
    // Not enough continuous observation of the cooling body yet to judge the
    // trend (after configuration or a gap): never an `Ok`, the Peltier stays
    // blocked until the observation covers at least half the trend window.
    TrendObservationPending,
};

[[nodiscard]] const char* thermalSafetyReasonName(
    ThermalSafetyReason reason) noexcept;
[[nodiscard]] const char* thermalSafetyStatusName(
    ThermalSafetyStatus status) noexcept;

// Owner-approved limits (trial: G-P1 for C1, G-P2 for C2). Two levels per
// sensor: intervention (inner) and hard emergency (outer). Both trip and latch;
// the counter-direction recovery attempts of the specification are not
// implemented (trial attempt count 0).
struct ThermalSafetyLimits {
    double airEmergencyLowerCelsius{0.0};
    double airInterventionLowerCelsius{0.0};
    double airInterventionUpperCelsius{0.0};
    double airEmergencyUpperCelsius{0.0};
    double coolingEmergencyLowerCelsius{0.0};
    double coolingInterventionLowerCelsius{0.0};
    double coolingInterventionUpperCelsius{0.0};
    double coolingEmergencyUpperCelsius{0.0};
    // Maximum sustained temperature rise of the cooling body.
    double coolingMaxRiseCelsiusPerMinute{0.0};
    std::uint64_t trendWindowMillis{0U};
    // A reading older than this is stale (Peltier blocked).
    std::uint64_t maxSampleAgeMillis{0U};
    // Invalid readings for this long latch the supervisor.
    std::uint64_t invalidLatchMillis{0U};
};

enum class ThermalSafetyConfigureResult : std::uint8_t {
    Accepted,
    InvalidLimits,
    Latched,
    // Application level: a run is active or the run state is unknown. The
    // protection must not be reconfigured while a run (power phase) exists.
    RunActiveOrUnknown,
};

// Firmware-fixed outer bounds of every parameterisable value.
inline constexpr std::uint64_t kThermalSafetyMinTrendWindowMillis = 10'000U;
inline constexpr std::uint64_t kThermalSafetyMaxTrendWindowMillis = 600'000U;
inline constexpr std::uint64_t kThermalSafetyMinSampleAgeMillis = 1'000U;
inline constexpr std::uint64_t kThermalSafetyMaxSampleAgeMillis = 60'000U;
inline constexpr std::uint64_t kThermalSafetyMaxInvalidLatchMillis = 600'000U;
inline constexpr double kThermalSafetyMaxRiseCelsiusPerMinute = 100.0;

[[nodiscard]] bool validateThermalSafetyLimits(
    const ThermalSafetyLimits& limits) noexcept;

struct ThermalSafetyEvaluation {
    ThermalSafetyStatus status{ThermalSafetyStatus::Unconfigured};
    ThermalSafetyReason reason{ThermalSafetyReason::NotConfigured};
};

class ThermalSafetySupervisor final {
   public:
    // Validates against the firmware-fixed outer bounds (finite, ordered,
    // inside the DS18B20 measuring range, bounded windows). Refused while
    // latched: the latch is cleared only by a restart (volatile).
    [[nodiscard]] ThermalSafetyConfigureResult configure(
        const ThermalSafetyLimits& limits) noexcept;
    // Removes the configuration (never clears a latch).
    void clearConfiguration() noexcept;

    // Idempotent for identical samples and time.
    ThermalSafetyEvaluation evaluate(const RawTemperatureSamples& samples,
                                     std::uint64_t nowMonotonicMillis);

    [[nodiscard]] ThermalSafetyEvaluation last() const noexcept {
        return last_;
    }
    [[nodiscard]] bool configured() const noexcept { return configured_; }
    [[nodiscard]] bool latched() const noexcept { return latched_.has_value(); }
    [[nodiscard]] const ThermalSafetyLimits& limits() const noexcept {
        return limits_;
    }

   private:
    // Time-bucketed trend history: each slot keeps the FIRST reading of one
    // time bucket of width ceil(window / 30). 32 slots cover the longest
    // permitted window (600 s) with any sample interval (2 s or irregular) in
    // 768 bytes; a plain sample ring would only reach ~126 s at 2 s cycles.
    struct TrendSlot {
        std::uint64_t bucketIndex{0U};
        std::uint64_t timestampMillis{0U};
        double celsius{0.0};
        bool used{false};
    };
    static constexpr std::size_t kTrendSlots = 32U;
    static constexpr std::uint64_t kTrendBucketsPerWindow = 30U;

    ThermalSafetyEvaluation trip(ThermalSafetyReason reason) noexcept;
    void resetTrend() noexcept;
    void recordTrend(std::uint64_t timestampMillis, double celsius) noexcept;
    enum class TrendJudgement : std::uint8_t { Pending, Within, Exceeded };
    [[nodiscard]] TrendJudgement judgeTrend(
        std::uint64_t newestMillis, double newestCelsius) const noexcept;

    bool configured_{false};
    ThermalSafetyLimits limits_{};
    std::optional<ThermalSafetyReason> latched_;
    std::optional<std::uint64_t> invalidSinceMillis_;
    std::array<TrendSlot, kTrendSlots> trend_{};
    std::uint64_t trendBucketWidthMillis_{1U};
    std::optional<std::uint64_t> trendFirstMillis_;
    std::optional<std::uint64_t> trendLastMillis_;
    ThermalSafetyEvaluation last_{};
};

}  // namespace fermentation
