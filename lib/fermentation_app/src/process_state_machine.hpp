#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "process_signal_types.hpp"
#include "program_model.hpp"
#include "run_snapshot.hpp"
#include "control_context_types.hpp"

namespace fermentation {

enum class ProcessState : std::uint8_t {
    Boot,
    SafeBoot,
    Standby,
    Preheating,
    WaitingForProduct,
    ReachingTarget,
    QualifyingTarget,
    Fermenting,
    Cooling,
    CoolHolding,
    ManualHolding,
    Completed,
    RecoveryEvaluation,
    Fault,
    ServiceMode,
};

enum class ProcessKind : std::uint8_t {
    Timed,
    ManualHolding,
};

struct ProcessRunSnapshot {
    ProcessKind kind{ProcessKind::Timed};
    bool preheatEnabled{false};
    CompletionMode completionMode{CompletionMode::FinishWithoutCooling};
    std::uint32_t qualificationDurationMinutes{0U};
    std::uint32_t maximumTargetReachMinutes{0U};
    std::optional<std::uint32_t> maximumProductWaitMinutes;
    std::optional<std::uint32_t> fermentationDurationMinutes;
    std::optional<std::uint32_t> holdDurationMinutes;
};

[[nodiscard]] bool validateProcessRunSnapshot(
    const ProcessRunSnapshot& snapshot);
[[nodiscard]] std::optional<ProcessRunSnapshot> makeProcessRunSnapshot(
    const ActiveRun& run);
[[nodiscard]] bool equalProcessRunSnapshot(const ProcessRunSnapshot& left,
                                           const ProcessRunSnapshot& right);

// Oeffentlich fuer den Schema-3-Gueltigkeitsvertrag ausserhalb dieser
// Uebersetzungseinheit (run_persistence_contract.cpp, 5.14): prueft, ob eine
// Phase strukturell zu einem gegebenen Programm-/manuellen Schnappschuss
// passt, unabhaengig von einer konkreten ProcessRuntimeState-Instanz.
[[nodiscard]] bool stateMatchesRunSnapshot(ProcessState state,
                                           const ProcessRunSnapshot& snapshot);

// Oeffentlich fuer denselben Schema-3-Gueltigkeitsvertrag (5.14
// Korrekturauftrag Befund 1): stateMatchesRunSnapshot() liefert fuer nicht
// run-gebundene Zustaende (Boot/Standby/Completed/RecoveryEvaluation/Fault/
// ServiceMode) absichtlich true und kann eine Recovery-Altphase deshalb
// nicht allein pruefen - stateUsesRunSnapshot() grenzt zusaetzlich auf genau
// die acht Phasen ein, die ueberhaupt eine laufende, snapshot-gebundene
// Regelung darstellen.
[[nodiscard]] bool stateUsesRunSnapshot(ProcessState state);

struct ProcessRuntimeState {
    ProcessState state{ProcessState::Boot};
    std::uint64_t stateEnteredAtMillis{0U};
    std::uint64_t targetReachStartedAtMillis{0U};
    std::optional<std::uint64_t> qualificationValidSinceMillis;
    bool targetReachWarningIssued{false};
    std::uint32_t transitionSequence{0U};
};

[[nodiscard]] bool equalProcessRuntimeState(const ProcessRuntimeState& left,
                                            const ProcessRuntimeState& right);
[[nodiscard]] bool validateProcessRuntimeForCheckpoint(
    const ProcessRuntimeState& state, const ProcessRunSnapshot* runSnapshot,
    std::uint64_t checkpointMonotonicMillis);

enum class ProcessEvent : std::uint8_t {
    None,
    StartRun,
    ProductInsertedConfirmed,
    TargetChanged,
    Abort,
    FinishHoldConfirmed,
    AcknowledgeCompletion,
    EnterServiceMode,
    ExitServiceMode,
    BootReady,
    BootSafe,
    BootRestoreCompleted,
    BootRecoverRun,
    RecoveryResume,
    RecoveryReject,
};

struct TransitionRequest {
    ProcessEvent event{ProcessEvent::None};
    std::optional<ProcessRuntimeState> recoveredState;
};

enum class DecisionStatus : std::uint8_t {
    Proposed,
    NoTransition,
    Rejected,
    InvalidInput,
    TimeWentBackwards,
};

enum class TransitionReason : std::uint8_t {
    None,
    BootCompleted,
    SafeBootRequired,
    CompletedRunRestored,
    RecoveryRequired,
    RunStarted,
    QualificationTrackingStarted,
    QualificationReset,
    PreheatQualified,
    ProductInserted,
    TargetChangedReevaluation,
    ProductWaitExpired,
    TargetReachTimeExceeded,
    TargetQualified,
    FermentationCompleted,
    CoolingTargetReached,
    HoldDurationCompleted,
    HoldFinishedByUser,
    RunAborted,
    CompletionAcknowledged,
    CriticalFault,
    ServiceModeEntered,
    ServiceModeExited,
    RecoveryResumed,
    RecoveryRejected,
    RecoveryReentryRequired,
    RecoveryEndedByExpiredWait,
};

enum class ProcessMessage : std::uint8_t {
    ProductInsertionRequested,
    TargetReachTimeExceeded,
    RunCompleted,
    RunAborted,
    FaultEntered,
};

inline constexpr std::size_t kMaximumTransitionMessages = 3U;

struct TransitionDecision {
    DecisionStatus status{DecisionStatus::NoTransition};
    ProcessRuntimeState before;
    ProcessRuntimeState after;
    TransitionReason reason{TransitionReason::None};
    std::uint64_t monotonicMillis{0U};
    std::array<ProcessMessage, kMaximumTransitionMessages> messages{};
    std::size_t messageCount{0U};
    // Fluessige Commit-Information fuer den #22-Integrator. Sie wird erst
    // nach erfolgreichem applyProcessTransition wirksam und ist kein
    // Prozess-/Wirefeld.
    std::optional<CommittedControlContextTransition>
        committedControlContextTransition;

    [[nodiscard]] bool proposed() const {
        return status == DecisionStatus::Proposed;
    }
};

// Bereits bekannter Vor-Boot-Anteil einer Phase (Recovery), boot-unabhaengig
// und additiv gefuehrt statt durch Zurueckrechnen des Boot-Zeitpunkts.
struct PriorBootPhaseElapsed {
    std::uint32_t lowerBoundSeconds{0U};
    std::optional<std::uint32_t> upperBoundSeconds;
};

// now >= startedAt ist innerhalb desselben Boots durch propose()/
// Hop-1-Konstruktion garantiert - keine Unterlaufgefahr, da hier
// ausschliesslich addiert, nie von now subtrahiert wird.
[[nodiscard]] inline bool elapsedWithPrior(std::uint64_t now,
                                           std::uint64_t startedAt,
                                           std::uint32_t durationMinutes,
                                           std::uint32_t priorSeconds) {
    return (now - startedAt) / 1000U + priorSeconds >=
           static_cast<std::uint64_t>(durationMinutes) * 60U;
}

[[nodiscard]] TransitionDecision decideProcessTransition(
    const ProcessRuntimeState& current, const ProcessRunSnapshot* runSnapshot,
    const ProcessSignals& signals, const TransitionRequest& request,
    std::uint64_t monotonicMillis,
    const PriorBootPhaseElapsed& priorElapsed = {});

[[nodiscard]] bool applyProcessTransition(
    ProcessRuntimeState& current, const TransitionDecision& decision,
    const ProcessRunSnapshot* runSnapshot);

// Oeffentlich fuer Recovery-Orchestrierung ausserhalb dieser
// Uebersetzungseinheit
// (RunPersistenceCoordinator::activateLoadedRun/activateFallbackRecoveredRun,
// RunPersistenceCoordinator::resolveRecoveryOutcome): konstruiert dieselbe
// Transitionsform wie decideProcessTransition, ohne den vollen
// Entscheidungspfad zu durchlaufen.
[[nodiscard]] TransitionDecision propose(const ProcessRuntimeState& current,
                                         ProcessState nextState,
                                         TransitionReason reason,
                                         std::uint64_t monotonicMillis);

[[nodiscard]] TransitionDecision completeTimedRun(
    const ProcessRuntimeState& current, const ProcessRunSnapshot& snapshot,
    std::uint64_t monotonicMillis);

[[nodiscard]] TransitionDecision completeHoldDuration(
    const ProcessRuntimeState& current, std::uint64_t monotonicMillis);

}  // namespace fermentation
