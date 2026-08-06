#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "sensor_limits.hpp"

// Vollstaendige, gueltig-by-construction Parametrierung einer einzelnen
// SensorQualityPipeline-Instanz. Siehe
// docs/tasks/issue-20-sensor-quality-filtering-plan.md, Abschnitt 10.0, fuer
// die vollstaendige Herleitung der Validierungsreihenfolge und -gruende.
namespace device_platform {

enum class SensorQualityConfigStatus : uint8_t {
    Success,
    // medianWindowSize == 0, gerade, oder >
    // sensor_limits::kMaxMedianWindowSize.
    InvalidMedianWindowSize,
    // Mindestens einer der double-Parameter ist NaN oder Inf. Wird VOR jeder
    // Bereichs-/Beziehungspruefung erkannt, da z. B. `NaN >= x` immer false
    // ist und eine Bereichsverletzung sonst unbemerkt bliebe.
    NonFiniteParameter,
    // lowPassTauSeconds <= 0.0.
    InvalidLowPassTimeConstant,
    // minPlausibleCelsius >= maxPlausibleCelsius, oder ausserhalb der
    // firmwarefesten Aussengrenze [kAbsoluteMinCelsius, kAbsoluteMaxCelsius].
    InvalidPlausibleRange,
    // maxRateOfChangeCelsiusPerSecond <= 0.0.
    InvalidRateOfChangeLimit,
    // maxStaleAgeMs == 0, oder > sensor_limits::kMaxStaleAgeCeilingMs.
    InvalidStaleAgeThreshold,
    // maxConsecutiveInvalid == 0, oder >
    // sensor_limits::kMaxConsecutiveInvalidCeiling.
    InvalidConsecutiveInvalidLimit,
    // minConsecutiveValidSamples < 2 (die kanonische Spezifikation verlangt
    // "mehrere gueltige Proben" - eine Einzelprobe ist keine Mehrzahl) ODER
    // > sensor_limits::kMaxConsecutiveValidSamplesCeiling, ODER
    // minRecoveryStabilityDurationMs == 0, ODER minRecoveryStabilityDurationMs
    // > sensor_limits::kMaxRecoveryStabilityDurationCeilingMs.
    InvalidRecoveryThresholds,
};

struct SensorQualityConfigCreateResult;

class SensorQualityConfig {
   public:
    [[nodiscard]] static SensorQualityConfigCreateResult create(
        std::size_t medianWindowSize, double lowPassTauSeconds,
        double minPlausibleCelsius, double maxPlausibleCelsius,
        double maxRateOfChangeCelsiusPerSecond, uint64_t maxStaleAgeMs,
        uint16_t maxConsecutiveInvalid, uint16_t minConsecutiveValidSamples,
        uint64_t minRecoveryStabilityDurationMs);

    [[nodiscard]] std::size_t medianWindowSize() const {
        return medianWindowSize_;
    }
    [[nodiscard]] double lowPassTauSeconds() const {
        return lowPassTauSeconds_;
    }
    [[nodiscard]] double minPlausibleCelsius() const {
        return minPlausibleCelsius_;
    }
    [[nodiscard]] double maxPlausibleCelsius() const {
        return maxPlausibleCelsius_;
    }
    [[nodiscard]] double maxRateOfChangeCelsiusPerSecond() const {
        return maxRateOfChangeCelsiusPerSecond_;
    }
    [[nodiscard]] uint64_t maxStaleAgeMs() const { return maxStaleAgeMs_; }
    [[nodiscard]] uint16_t maxConsecutiveInvalid() const {
        return maxConsecutiveInvalid_;
    }
    [[nodiscard]] uint16_t minConsecutiveValidSamples() const {
        return minConsecutiveValidSamples_;
    }
    [[nodiscard]] uint64_t minRecoveryStabilityDurationMs() const {
        return minRecoveryStabilityDurationMs_;
    }

   private:
    SensorQualityConfig(std::size_t medianWindowSize, double lowPassTauSeconds,
                        double minPlausibleCelsius, double maxPlausibleCelsius,
                        double maxRateOfChangeCelsiusPerSecond,
                        uint64_t maxStaleAgeMs, uint16_t maxConsecutiveInvalid,
                        uint16_t minConsecutiveValidSamples,
                        uint64_t minRecoveryStabilityDurationMs)
        : medianWindowSize_(medianWindowSize),
          lowPassTauSeconds_(lowPassTauSeconds),
          minPlausibleCelsius_(minPlausibleCelsius),
          maxPlausibleCelsius_(maxPlausibleCelsius),
          maxRateOfChangeCelsiusPerSecond_(maxRateOfChangeCelsiusPerSecond),
          maxStaleAgeMs_(maxStaleAgeMs),
          maxConsecutiveInvalid_(maxConsecutiveInvalid),
          minConsecutiveValidSamples_(minConsecutiveValidSamples),
          minRecoveryStabilityDurationMs_(minRecoveryStabilityDurationMs) {}

    std::size_t medianWindowSize_;
    double lowPassTauSeconds_;
    double minPlausibleCelsius_;
    double maxPlausibleCelsius_;
    double maxRateOfChangeCelsiusPerSecond_;
    uint64_t maxStaleAgeMs_;
    uint16_t maxConsecutiveInvalid_;
    uint16_t minConsecutiveValidSamples_;
    uint64_t minRecoveryStabilityDurationMs_;
};

struct SensorQualityConfigCreateResult {
    SensorQualityConfigStatus status{
        SensorQualityConfigStatus::InvalidMedianWindowSize};
    std::optional<SensorQualityConfig> config;
};

inline SensorQualityConfigCreateResult SensorQualityConfig::create(
    std::size_t medianWindowSize, double lowPassTauSeconds,
    double minPlausibleCelsius, double maxPlausibleCelsius,
    double maxRateOfChangeCelsiusPerSecond, uint64_t maxStaleAgeMs,
    uint16_t maxConsecutiveInvalid, uint16_t minConsecutiveValidSamples,
    uint64_t minRecoveryStabilityDurationMs) {
    // (1) Nicht endliche double-Parameter zuerst - vor jeder Bereichs-/
    // Beziehungspruefung, siehe NonFiniteParameter-Kommentar oben.
    if (!std::isfinite(lowPassTauSeconds) ||
        !std::isfinite(minPlausibleCelsius) ||
        !std::isfinite(maxPlausibleCelsius) ||
        !std::isfinite(maxRateOfChangeCelsiusPerSecond)) {
        return SensorQualityConfigCreateResult{
            SensorQualityConfigStatus::NonFiniteParameter, std::nullopt};
    }
    // (2) Medianfenstergroesse: ungleich 0, ungerade, innerhalb der
    // firmwarefesten Obergrenze.
    if (medianWindowSize == 0U || (medianWindowSize % 2U) == 0U ||
        medianWindowSize > sensor_limits::kMaxMedianWindowSize) {
        return SensorQualityConfigCreateResult{
            SensorQualityConfigStatus::InvalidMedianWindowSize, std::nullopt};
    }
    // (3) Tiefpass-Zeitkonstante.
    if (lowPassTauSeconds <= 0.0) {
        return SensorQualityConfigCreateResult{
            SensorQualityConfigStatus::InvalidLowPassTimeConstant,
            std::nullopt};
    }
    // (4) Plausibilitaetsband: geordnet und innerhalb der firmwarefesten
    // Aussengrenze.
    if (minPlausibleCelsius >= maxPlausibleCelsius ||
        minPlausibleCelsius < sensor_limits::kAbsoluteMinCelsius ||
        maxPlausibleCelsius > sensor_limits::kAbsoluteMaxCelsius) {
        return SensorQualityConfigCreateResult{
            SensorQualityConfigStatus::InvalidPlausibleRange, std::nullopt};
    }
    // (5) Aenderungsratenlimit.
    if (maxRateOfChangeCelsiusPerSecond <= 0.0) {
        return SensorQualityConfigCreateResult{
            SensorQualityConfigStatus::InvalidRateOfChangeLimit, std::nullopt};
    }
    // (6) Stale-Altersschwelle.
    if (maxStaleAgeMs == 0U ||
        maxStaleAgeMs > sensor_limits::kMaxStaleAgeCeilingMs) {
        return SensorQualityConfigCreateResult{
            SensorQualityConfigStatus::InvalidStaleAgeThreshold, std::nullopt};
    }
    // (7) Obergrenze aufeinanderfolgender ungueltiger Proben.
    if (maxConsecutiveInvalid == 0U ||
        maxConsecutiveInvalid > sensor_limits::kMaxConsecutiveInvalidCeiling) {
        return SensorQualityConfigCreateResult{
            SensorQualityConfigStatus::InvalidConsecutiveInvalidLimit,
            std::nullopt};
    }
    // (8) Wiedererkennungsschwellen: mehrere gueltige Proben (>= 2,
    // firmwarefest begrenzte Obergrenze) UND eine echte, firmwarefest begrenzte
    // Stabilitaetszeit.
    if (minConsecutiveValidSamples < 2U ||
        minConsecutiveValidSamples >
            sensor_limits::kMaxConsecutiveValidSamplesCeiling ||
        minRecoveryStabilityDurationMs == 0U ||
        minRecoveryStabilityDurationMs >
            sensor_limits::kMaxRecoveryStabilityDurationCeilingMs) {
        return SensorQualityConfigCreateResult{
            SensorQualityConfigStatus::InvalidRecoveryThresholds, std::nullopt};
    }
    return SensorQualityConfigCreateResult{
        SensorQualityConfigStatus::Success,
        SensorQualityConfig(medianWindowSize, lowPassTauSeconds,
                            minPlausibleCelsius, maxPlausibleCelsius,
                            maxRateOfChangeCelsiusPerSecond, maxStaleAgeMs,
                            maxConsecutiveInvalid, minConsecutiveValidSamples,
                            minRecoveryStabilityDurationMs)};
}

}  // namespace device_platform
