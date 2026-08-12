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

struct TargetQualificationContext {
    double targetCelsius{0.0};
    double bandCelsius{0.0};
    ControlSensorRole controlSensorRole{ControlSensorRole::Air};
};

[[nodiscard]] inline bool operator==(const TargetQualificationContext& left,
                                     const TargetQualificationContext& right) {
    return left.targetCelsius == right.targetCelsius &&
           left.bandCelsius == right.bandCelsius &&
           left.controlSensorRole == right.controlSensorRole;
}

struct TargetQualificationRuntimeState {
    QualificationPhase phase{QualificationPhase::Target};
    std::optional<TargetQualificationContext> context;
    bool episodeActive{false};
    std::uint64_t creditedInBandMillis{0U};
    std::optional<std::uint64_t> lastUsableTimestampMillis;
    std::optional<std::uint64_t> graceStartedAtMillis;
};

struct TargetQualificationResult {
    QualificationProgress progress{QualificationProgress::Unavailable};
    std::uint64_t creditedInBandMillis{0U};
    TargetQualificationRuntimeState expectedEvaluatorState;
    TargetQualificationRuntimeState candidateEvaluatorState;
};

enum class TargetQualificationApplyStatus : std::uint8_t {
    AppliedRamOnly,
    PersistedAndProcessApplied,
    PersistenceFailed,
    ProcessApplyFailed,
    StaleDecision,
};

class TargetQualificationEvaluator {
   public:
    [[nodiscard]] TargetQualificationResult evaluate(
        const TargetQualificationInput& input);

    // The caller supplies the outcome of the existing write-before-apply
    // process path. The evaluator never persists its RAM-only state.
    [[nodiscard]] bool apply(const TargetQualificationResult& decision,
                             TargetQualificationApplyStatus status);

    void reset();

    [[nodiscard]] const TargetQualificationRuntimeState& state() const {
        return state_;
    }

   private:
    TargetQualificationRuntimeState state_;
};

}  // namespace fermentation
