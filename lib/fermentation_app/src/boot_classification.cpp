#include "boot_classification.hpp"

namespace fermentation::boot_classification {

bool isR1ResumeEligible(const RunPersistenceSnapshot& snapshot) noexcept {
    if (snapshot.variant == RunCheckpointVariant::NoActiveRun ||
        snapshot.processState.state == ProcessState::Completed ||
        snapshot.processState.state == ProcessState::Fault ||
        snapshot.processState.state == ProcessState::RecoveryEvaluation ||
        snapshot.pendingRecoveryAnchor.has_value() ||
        snapshot.recoveryBootAnchorMonotonicMillis.has_value() ||
        snapshot.lastRecoveryEpisodeEvidence.has_value() ||
        snapshot.priorBootPhaseElapsed.has_value() ||
        snapshot.nominalRecoveryAdjustment.has_value() ||
        snapshot.runProgress.weightedProgress.has_value() ||
        snapshot.runProgress.basis == RunProgressBasis::PartialUnknownHistory) {
        return false;
    }
    switch (snapshot.processState.state) {
        case ProcessState::Preheating:
        case ProcessState::Cooling:
        case ProcessState::ManualHolding:
            return true;
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::CoolHolding:
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return false;
    }
    return false;
}

RunLoadDisposition classifyRunLoad(
    RunPersistenceLoadStatus status,
    const RunPersistenceSnapshot* snapshot) noexcept {
    switch (status) {
        case RunPersistenceLoadStatus::NoPersistedRun:
        case RunPersistenceLoadStatus::NoActiveRun:
            return RunLoadDisposition::Standby;
        case RunPersistenceLoadStatus::Current:
            if (snapshot == nullptr) return RunLoadDisposition::SafeBoot;
            if (snapshot->processState.state == ProcessState::Completed)
                return RunLoadDisposition::Completed;
            if (snapshot->processState.state == ProcessState::Fault)
                return RunLoadDisposition::TerminalFault;
            // A valid Current FERMENTING record is evaluated against trusted
            // UTC by the application. It is neither a user-facing ResumeOffer
            // nor a discardable run. The load path has already validated the
            // complete record graph; the evaluator applies the stricter R1
            // exact-time gates before any candidate can be committed.
            if (snapshot->processState.state == ProcessState::Fermenting)
                return RunLoadDisposition::RecoveryEvaluation;
            return isR1ResumeEligible(*snapshot)
                       ? RunLoadDisposition::ResumeOffer
                       : RunLoadDisposition::NoActiveRun;
        case RunPersistenceLoadStatus::FallbackRecovered:
            return snapshot == nullptr ? RunLoadDisposition::SafeBoot
                                       : RunLoadDisposition::FallbackSelectionRequired;
        case RunPersistenceLoadStatus::PreparedInterrupted:
        case RunPersistenceLoadStatus::NotReconstructible:
        case RunPersistenceLoadStatus::NotReconstructibleOrphanedState:
        case RunPersistenceLoadStatus::ReadFailed:
        case RunPersistenceLoadStatus::CapacityExceeded:
        case RunPersistenceLoadStatus::UnsupportedSchema:
        case RunPersistenceLoadStatus::ForeignEpoch:
        case RunPersistenceLoadStatus::AlreadyInitialized:
            return RunLoadDisposition::SafeBoot;
    }
    return RunLoadDisposition::SafeBoot;
}

BootClassification classify(RunPersistenceLoadStatus status,
                            const RunPersistenceSnapshot* snapshot) noexcept {
    switch (classifyRunLoad(status, snapshot)) {
        case RunLoadDisposition::Standby:
            return BootClassification::NoRun;
        case RunLoadDisposition::ResumeOffer:
            return BootClassification::ResumeOffer;
        case RunLoadDisposition::RecoveryEvaluation:
            return BootClassification::RecoveryEvaluation;
        case RunLoadDisposition::FallbackSelectionRequired:
            return BootClassification::FallbackSelectionRequired;
        case RunLoadDisposition::NoActiveRun:
            return BootClassification::DiscardableRun;
        case RunLoadDisposition::Completed:
            return BootClassification::CompletedRun;
        case RunLoadDisposition::TerminalFault:
            return BootClassification::TerminalRunFault;
        case RunLoadDisposition::SafeBoot:
            return BootClassification::SafeBoot;
    }
    return BootClassification::SafeBoot;
}

}  // namespace fermentation::boot_classification
