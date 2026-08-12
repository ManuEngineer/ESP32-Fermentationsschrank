#pragma once

#include <cstdint>
#include <optional>

#include "process_signal_types.hpp"
#include "sensor_quality_snapshot.hpp"
#include "temperature_control_types.hpp"

namespace fermentation {

enum class QualificationPhase : std::uint8_t {
    Preheating,
    Target,
};

struct TargetQualificationInput {
    QualificationPhase phase{QualificationPhase::Target};
    std::uint64_t sampleTimestampMonotonicMillis{0U};
    double targetCelsius{0.0};
    double bandCelsius{0.0};
    std::uint64_t qualificationDurationMillis{0U};
    std::optional<std::uint64_t> effectiveGraceMillis;
    std::optional<std::uint64_t> maximumAcceptedSampleGapMillis;
    ControlSensorRole controlSensorRole{ControlSensorRole::Air};
    device_platform::SensorQualitySnapshot air;
    device_platform::SensorQualitySnapshot product;
};

struct TargetQualificationRuntimeState {
    QualificationPhase phase{QualificationPhase::Target};
    bool episodeActive{false};
    std::uint64_t creditedInBandMillis{0U};
    std::optional<std::uint64_t> lastUsableTimestampMillis;
    std::optional<std::uint64_t> graceStartedAtMillis;
};

struct TargetQualificationResult {
    QualificationProgress progress{QualificationProgress::Unavailable};
    std::uint64_t creditedInBandMillis{0U};
    std::optional<std::uint64_t> qualificationValidSinceMillis;
};

class TargetQualificationEvaluator {
   public:
    [[nodiscard]] TargetQualificationResult evaluate(
        const TargetQualificationInput& input);

    void reset();

    [[nodiscard]] const TargetQualificationRuntimeState& state() const {
        return state_;
    }

   private:
    TargetQualificationRuntimeState state_;
};

}  // namespace fermentation
