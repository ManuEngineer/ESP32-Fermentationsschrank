#pragma once

#include <cstdint>
#include <optional>

#include "target_qualification.hpp"
#include "temperature_control_orchestrator.hpp"

namespace fermentation {

enum class TargetQualificationOrchestrationStatus : std::uint8_t {
    AppliedRamOnly,
    AppliedPersisted,
    CandidateDiscarded,
    StaleDecision,
    PersistenceFailed,
    InvalidDecision,
};

struct TargetQualificationOrchestrationResult {
    TargetQualificationResult qualification;
    ProcessSignals signals;
    std::optional<TransitionDecision> processDecision;
    std::optional<RunPersistenceResultStatus> persistenceStatus;
    TargetQualificationOrchestrationStatus status{
        TargetQualificationOrchestrationStatus::InvalidDecision};
};

// The only production bridge from qualification evidence to the existing
// process/persistence path. It never persists evaluator RAM state itself.
[[nodiscard]] TargetQualificationOrchestrationResult
evaluateAndApplyTargetQualification(
    TemperatureControlApplicationOrchestrator& application,
    RunCommandState& current, const TargetQualificationInput& input,
    const RunCheckpointTime& time, const ProcessSignals& baselineSignals = {},
    const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);

}  // namespace fermentation
