#pragma once

#include <cstdint>
#include <optional>

#include "control_context_types.hpp"
#include "process_signal_types.hpp"
#include "sensor_quality_snapshot.hpp"

namespace fermentation {

enum class QualificationPhase : std::uint8_t {
    Preheating,
    Target,
};

struct TargetQualificationInput {
    QualificationPhase phase{QualificationPhase::Target};
    std::uint64_t sampleTimestampMonotonicMillis{0U};
    std::uint32_t runRevision{0U};
    std::uint32_t processTransitionSequence{0U};
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

[[nodiscard]] inline bool operator!=(const TargetQualificationContext& left,
                                     const TargetQualificationContext& right) {
    return !(left == right);
}

struct TargetQualificationCommitContext {
    TargetQualificationContext qualification;
    std::uint32_t runRevision{0U};
    std::uint32_t processTransitionSequence{0U};
};

[[nodiscard]] inline bool operator==(
    const TargetQualificationCommitContext& left,
    const TargetQualificationCommitContext& right) {
    return left.qualification == right.qualification &&
           left.runRevision == right.runRevision &&
           left.processTransitionSequence == right.processTransitionSequence;
}

[[nodiscard]] inline bool operator!=(
    const TargetQualificationCommitContext& left,
    const TargetQualificationCommitContext& right) {
    return !(left == right);
}

struct TargetQualificationRuntimeState {
    QualificationPhase phase{QualificationPhase::Target};
    std::optional<TargetQualificationContext> context;
    std::optional<TargetQualificationCommitContext> commitContext;
    bool episodeActive{false};
    std::uint64_t creditedInBandMillis{0U};
    std::optional<std::uint64_t> lastUsableTimestampMillis;
    std::optional<std::uint64_t> graceStartedAtMillis;
};

enum class TargetQualificationDecisionLifecycle : std::uint8_t {
    Pending,
    Applied,
    Discarded,
};

struct TargetQualificationResult {
    QualificationProgress progress{QualificationProgress::Unavailable};
    std::uint64_t creditedInBandMillis{0U};
    TargetQualificationRuntimeState expectedEvaluatorState;
    TargetQualificationRuntimeState candidateEvaluatorState;
    TargetQualificationDecisionLifecycle lifecycle{
        TargetQualificationDecisionLifecycle::Pending};
    bool candidateApplicable{true};
};

[[nodiscard]] inline bool validQualificationPhase(QualificationPhase phase) {
    return phase == QualificationPhase::Preheating ||
           phase == QualificationPhase::Target;
}

class TargetQualificationEvaluator {
   public:
    [[nodiscard]] TargetQualificationResult evaluate(
        const TargetQualificationInput& input);

    // A decision is single-use. Failed/stale candidates are explicitly
    // discarded and cannot later be reinterpreted as successful applies.
    [[nodiscard]] bool applyRamOnly(
        TargetQualificationResult& decision,
        const TargetQualificationCommitContext& currentContext);
    [[nodiscard]] bool applyAfterPersistedProcessApply(
        TargetQualificationResult& decision,
        const TargetQualificationCommitContext& expectedBeforeContext,
        const TargetQualificationCommitContext& committedContext);
    void discard(TargetQualificationResult& decision);

    void reset();

    [[nodiscard]] const TargetQualificationRuntimeState& state() const {
        return state_;
    }

   private:
    TargetQualificationRuntimeState state_;
};

}  // namespace fermentation
