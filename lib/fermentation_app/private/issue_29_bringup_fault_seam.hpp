#pragma once

#if !defined(APP_ISSUE_29_BRINGUP_PROBE)
#error "Issue 29 fault seam is only available in esp32_bringup"
#endif

#include "run_commands.hpp"

namespace fermentation::issue_29_bringup {

[[nodiscard]] CommandStatus applyCandidateForResourceProbe(
    RunCommandState& candidate, const CommandDecision& decision) noexcept;

// This is a single, private diagnostic seam. It is intentionally not part of
// the production command, persistence, or allocator contracts. The probe
// arms it immediately before the existing candidate-copy boundary; the
// coordinator consumes it once and returns through its existing fail-closed
// result path.
inline bool candidateAllocationFailureArmed = false;

inline void armCandidateAllocationFailure() noexcept {
    candidateAllocationFailureArmed = true;
}

[[nodiscard]] inline bool consumeCandidateAllocationFailure() noexcept {
    const bool armed = candidateAllocationFailureArmed;
    candidateAllocationFailureArmed = false;
    return armed;
}

}  // namespace fermentation::issue_29_bringup
