#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "program_model.hpp"
#include "run_snapshot.hpp"

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

struct ProcessSignals {
    // Bezieht sich immer auf Ziel und Zielband der aktuellen Prozessphase.
    bool qualificationConditionValid{false};
    bool criticalFault{false};
};

enum class ProcessEvent : std::uint8_t {
    None,
    StartRun,
    ProductInsertedConfirmed,
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

    [[nodiscard]] bool proposed() const {
        return status == DecisionStatus::Proposed;
    }
};

[[nodiscard]] TransitionDecision decideProcessTransition(
    const ProcessRuntimeState& current, const ProcessRunSnapshot* runSnapshot,
    const ProcessSignals& signals, const TransitionRequest& request,
    std::uint64_t monotonicMillis);

[[nodiscard]] bool applyProcessTransition(
    ProcessRuntimeState& current, const TransitionDecision& decision,
    const ProcessRunSnapshot* runSnapshot);

}  // namespace fermentation
