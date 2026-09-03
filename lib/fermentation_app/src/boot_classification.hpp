#pragma once

#include <cstdint>

#include "run_persistence_coordinator.hpp"

namespace fermentation {

enum class BootClassification : std::uint8_t {
    Unresolved,
    NoRun,
    ResumeOffer,
    RecoveryEvaluation,
    FallbackSelectionRequired,
    DiscardableRun,
    CompletedRun,
    TerminalRunFault,
    SafeBoot,
};

enum class RunLoadDisposition : std::uint8_t {
    Standby,
    ResumeOffer,
    RecoveryEvaluation,
    FallbackSelectionRequired,
    NoActiveRun,
    Completed,
    TerminalFault,
    SafeBoot,
};

namespace boot_classification {

[[nodiscard]] bool isR1ResumeEligible(
    const RunPersistenceSnapshot& snapshot) noexcept;

[[nodiscard]] RunLoadDisposition classifyRunLoad(
    RunPersistenceLoadStatus status,
    const RunPersistenceSnapshot* snapshot) noexcept;

[[nodiscard]] BootClassification classify(
    RunPersistenceLoadStatus status,
    const RunPersistenceSnapshot* snapshot) noexcept;

}  // namespace boot_classification

}  // namespace fermentation
