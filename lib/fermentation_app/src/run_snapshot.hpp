#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "program_model.hpp"

namespace fermentation {

inline constexpr std::size_t kMaximumRunRevisions = 32U;

enum class ProgramSourceKind : std::uint8_t {
    FactoryCatalog,
    UserProgram,
};

enum class RunChangeSource : std::uint8_t {
    LocalDisplay,
    WebInterface,
    Recovery,
};

enum class RunChangeReason : std::uint8_t {
    UserAdjustment,
    RecoveryCorrection,
};

struct RunTimestamp {
    std::uint64_t monotonicMillis{0U};
    std::optional<std::int64_t> unixTimeSeconds;
};

struct EffectiveRunValues {
    double targetTemperatureCelsius{0.0};
    std::uint32_t remainingDurationMinutes{0U};
};

struct RunProgramSnapshot {
    ProgramDocument sourceProgram;
    ProgramSourceKind sourceKind{ProgramSourceKind::UserProgram};
    std::uint32_t sourceProgramRevision{0U};
};

struct RunRevision {
    std::uint32_t sequence{0U};
    std::uint32_t monotonicEpoch{0U};
    std::size_t stageIndex{0U};
    std::size_t completedStageCount{0U};
    EffectiveRunValues before;
    EffectiveRunValues after;
    bool targetTemperatureChanged{false};
    bool remainingDurationChanged{false};
    bool requiresTargetRequalification{false};
    RunChangeSource source{RunChangeSource::LocalDisplay};
    RunChangeReason reason{RunChangeReason::UserAdjustment};
    RunTimestamp timestamp;
};

struct RunAdjustmentContext {
    bool runActive{true};
    bool safetyAllowsChange{true};
    std::size_t activeStageIndex{0U};
    std::size_t completedStageCount{0U};
};

struct RunAdjustmentRequest {
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> remainingDurationMinutes;
    bool confirmed{false};
    RunChangeSource source{RunChangeSource::LocalDisplay};
    RunChangeReason reason{RunChangeReason::UserAdjustment};
    RunTimestamp timestamp;
};

enum class RunAdjustmentStatus : std::uint8_t {
    Proposed,
    Applied,
    NotConfirmed,
    NoChange,
    RunInactive,
    SafetyRejected,
    InvalidStage,
    CompletedStage,
    InvalidValue,
    InvalidMetadata,
    TimestampWentBackwards,
    RevisionCapacityReached,
};

struct RunAdjustmentDecision {
    RunAdjustmentStatus status{RunAdjustmentStatus::InvalidValue};
    std::size_t expectedRevisionCount{0U};
    EffectiveRunValues expectedValues;
    std::optional<RunRevision> revision;

    [[nodiscard]] bool proposed() const {
        return status == RunAdjustmentStatus::Proposed && revision.has_value();
    }
};

struct RunAdjustmentResult {
    RunAdjustmentStatus status{RunAdjustmentStatus::InvalidValue};
    bool requiresTargetRequalification{false};

    [[nodiscard]] bool applied() const {
        return status == RunAdjustmentStatus::Applied;
    }
};

class ActiveRun {
   public:
    [[nodiscard]] static std::optional<ActiveRun> start(
        const ProgramDocument& sourceProgram, ProgramSourceKind sourceKind,
        std::uint32_t sourceProgramRevision);

    [[nodiscard]] static std::optional<ActiveRun> restore(
        const RunProgramSnapshot& snapshot,
        const std::array<RunRevision, kMaximumRunRevisions>& revisions,
        std::size_t revisionCount);

    [[nodiscard]] RunAdjustmentDecision decideAdjustment(
        const RunAdjustmentRequest& request,
        const RunAdjustmentContext& context) const;
    [[nodiscard]] RunAdjustmentResult applyAdjustment(
        const RunAdjustmentDecision& decision);

    [[nodiscard]] const RunProgramSnapshot& snapshot() const;
    [[nodiscard]] const EffectiveRunValues& effectiveValues() const;
    [[nodiscard]] const std::array<RunRevision, kMaximumRunRevisions>&
    revisions() const;
    [[nodiscard]] std::size_t revisionCount() const;
    [[nodiscard]] const RunRevision* revisionAt(std::size_t index) const;

   private:
    ActiveRun(RunProgramSnapshot snapshot, EffectiveRunValues initialValues);

    RunProgramSnapshot snapshot_;
    EffectiveRunValues effectiveValues_;
    std::array<RunRevision, kMaximumRunRevisions> revisions_{};
    std::size_t revisionCount_{0U};
    std::uint32_t monotonicEpoch_{0U};
};

}  // namespace fermentation
