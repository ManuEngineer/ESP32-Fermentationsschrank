#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "control_context_types.hpp"
#include "run_checkpoint_schedule.hpp"
#include "run_persistence_contract.hpp"
#include "run_persistence_store.hpp"
#include "storage_types.hpp"

namespace fermentation {

enum class RunPersistenceCoordinatorState : std::uint8_t {
    Uninitialized,
    ReadyEmpty,
    LoadedActiveRun,
    Ready,
    Busy,
    BlockedIndeterminate,
    FallbackRecoveryPending,
    PersistenceCommittedApplyFailed,
};

enum class RunPersistenceResultStatus : std::uint8_t {
    Applied,
    CheckpointWritten,
    // #21, 9.7-Korrektur (letzter Abschlussblocker): dieselbe CommandId
    // eines RAM-only angewendeten ApplySensorSelectionAction (AppliedRamOnly)
    // wurde innerhalb desselben Boots bereits verarbeitet - im Unterschied zu
    // AlreadyPersisted ist das keine dauerhafte Sperre und ueberlebt keinen
    // Neustart, da RunCommandState::processedCommandIds RAM-only ist.
    AlreadyProcessed,
    AlreadyPersisted,
    NotEligible,
    NotAllowedInState,
    NotInitialized,
    RecoveryPending,
    Busy,
    InvalidDecision,
    StaleDecision,
    TimeMismatch,
    TimeWentBackwards,
    CounterOverflow,
    WriteFailed,
    CapacityExceeded,
    PersistenceIndeterminate,
    PersistenceCommittedApplyFailed,
    Blocked,
    NotDue,
    NoActiveRun,
};

enum class RunPersistenceStep : std::uint8_t {
    None,
    CandidateApply,
    PreparedHead,
    CheckpointSlot,
    CommittedHead,
    RamApply,
    LoadHead,
    LoadCurrent,
    LoadFallback,
};

enum class RunPersistenceTechnicalReason : std::uint8_t {
    None,
    StoreWriteError,
    StoreCapacityError,
    StoreReadError,
    StoreNotWritten,
    StoreOutcomeUnknown,
    CodecError,
    InvalidProjection,
};

enum class RunPersistenceDurability : std::uint8_t {
    Unchanged,
    Changed,
    MayHaveChanged,
};

struct RunPersistenceResult {
    RunPersistenceResultStatus status{RunPersistenceResultStatus::Blocked};
    RunPersistenceStep step{RunPersistenceStep::None};
    RunPersistenceTechnicalReason technicalReason{
        RunPersistenceTechnicalReason::None};
    RunPersistenceDurability durability{RunPersistenceDurability::Unchanged};
    RunPersistenceCoordinatorState coordinatorState{
        RunPersistenceCoordinatorState::Uninitialized};
    std::array<CommandEffect, run_command_limits::kMaximumCommandEffects>
        effects{};
    std::size_t effectCount{0U};
    std::array<ProcessMessage, kMaximumTransitionMessages> messages{};
    std::size_t messageCount{0U};
    // #21, 6.14.4: set only after a successful persistSensorSelection commit,
    // mutually exclusive (a mode change reports an event, everything else a
    // notice - never both, mirrors SensorSelectionStateMutation).
    std::optional<SensorSelectionEvent> sensorSelectionEvent{};
    std::optional<SensorSelectionNotice> sensorSelectionNotice{};
    // #21, 6.11: nur bei tatsaechlichem requestedMode != effectiveMode
    // (Startmatrix Zeile 2), erst nach erfolgreichem Commit sichtbar.
    std::optional<StartSensorSelectionNotice> startSensorSelectionNotice{};
    // Transient post-commit handoff only: set after successful persistence
    // and RAM apply for a target, sensor-role, cooling-target, or effective
    // ProductInserted Air -> Product context transition. Never persisted.
    std::optional<CommittedControlContextTransition>
        committedControlContextTransition;
};

enum class RunPersistenceLoadStatus : std::uint8_t {
    NoPersistedRun,
    Current,
    NoActiveRun,
    FallbackRecovered,
    PreparedInterrupted,
    NotReconstructible,
    NotReconstructibleOrphanedState,
    ReadFailed,
    CapacityExceeded,
    UnsupportedSchema,
    ForeignEpoch,
    AlreadyInitialized,
};

struct RunPersistenceLoadResult {
    RunPersistenceLoadStatus status{RunPersistenceLoadStatus::ReadFailed};
    std::optional<RunPersistenceSnapshot> snapshot;
};

struct RecoveryActivationOutcome {
    RunPersistenceResult persistenceResult;
    RunCommandState resultingState;
};

enum class RecoveryUncertaintyDecision : std::uint8_t {
    AssumeStillValid,
    AssumeThresholdCrossed,
};

struct ResolveRecoveryUncertaintyRequest {
    CommandId commandId{0U};
    std::uint32_t expectedRunRevision{0U};
    std::uint32_t expectedRecoveryEpisodeRevision{0U};
    RecoveryUncertaintyDecision decision{
        RecoveryUncertaintyDecision::AssumeStillValid};
};

enum class RunPersistenceFallbackMode : std::uint8_t {
    UseStandardFallback,
    SetExplicitReference,
    ClearFallback,
};

struct RunPersistenceFallbackDirective {
    RunPersistenceFallbackMode mode{
        RunPersistenceFallbackMode::UseStandardFallback};
    std::optional<RunCheckpointReference> reference;
};

// Reine RAM-Mutation fuer die diagnostische First-after-restart-Evidenz. Die
// Funktion ist unabhaengig davon aufrufbar, ob im selben Moment ein
// Persistenz-Commit stattfindet.
void applyLiveRecoveryEvidence(
    RunCommandState& current,
    const CrossRolePlausibilityContext& liveSensorEvidence);

class RunPersistenceCoordinator {
   public:
    // Private implementation data is declared here solely because the
    // canonical wire helpers in the implementation unit need their complete
    // names; callers cannot construct or obtain these types.
    RunPersistenceCoordinator(device_platform::IStateStore& store,
                              device_platform::StorageEpoch epoch,
                              RunCheckpointSchedule schedule) noexcept;
    ~RunPersistenceCoordinator() = default;
    RunPersistenceCoordinator(const RunPersistenceCoordinator&) = delete;
    RunPersistenceCoordinator& operator=(const RunPersistenceCoordinator&) =
        delete;
    RunPersistenceCoordinator(RunPersistenceCoordinator&&) = delete;
    RunPersistenceCoordinator& operator=(RunPersistenceCoordinator&&) = delete;

    [[nodiscard]] RunPersistenceLoadResult loadAndInitialize();
    [[nodiscard]] RunPersistenceResult persistCommand(
        RunCommandState& current, const CommandDecision& decision,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);
    [[nodiscard]] RunPersistenceResult persistTransition(
        RunCommandState& current, const TransitionDecision& decision,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);
    // #21, 6.14.3: transports exactly the six automatic
    // SensorSelectionDecisionCause values (never ManualUserFallback/
    // ManualUserReturn, which route through persistCommand once #21 Commit 4
    // adds the manual command path). `mutation` is the already-computed
    // decision from applySensorSelectionDecision (sensor_selection.hpp) -
    // this function contains no second rule implementation.
    [[nodiscard]] RunPersistenceResult persistSensorSelection(
        RunCommandState& current, const SensorSelectionStateMutation& mutation,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);
    [[nodiscard]] RunPersistenceResult checkpointPeriodic(
        const RunCommandState& current, const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);
    [[nodiscard]] RunPersistenceCoordinatorState state() const {
        return state_;
    }
    [[nodiscard]] RecoveryActivationOutcome activateLoadedRun(
        const RunCommandState& current, const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);
    [[nodiscard]] RecoveryActivationOutcome activateFallbackRecoveredRun(
        const RunCommandState& current, const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);
    [[nodiscard]] RunPersistenceResult resolveRecoveryOutcome(
        RunCommandState& current,
        const ResolveRecoveryUncertaintyRequest& request,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);
    // R1 discard path for a technically trusted but semantically non-resumable
    // run. The detached NoActiveRun candidate is durably committed first;
    // current RAM is changed only after the complete coordinator transaction
    // returns Applied. This does not repair an untrusted store state.
    [[nodiscard]] RunPersistenceResult discardAsNoActiveRun(
        RunCommandState& current, const RunCheckpointTime& time);
    // Automatic UTC reevaluation of the persisted Hop-1-only
    // WaitingForProduct case. Expiry is resolved without Gate A; a still
    // valid result requires the optional fresh Gate-A context.
    [[nodiscard]] RunPersistenceResult reevaluateRecoveryEvaluation(
        RunCommandState& current, const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);

    // Gemeinsamer, write-before-apply Recovery-Schreibpfad fuer spaetere
    // fachliche Recovery-Kandidaten (Zeit-Reevaluation und Gewichtung). Der
    // Aufrufer liefert bereits den vollstaendigen, um genau eine
    // RunRevision erhoehten Kandidaten; hier bleibt die Persistenzmutation
    // auf `Recovery` festgelegt und es entsteht kein zweiter Schreibkern.
    [[nodiscard]] RunPersistenceResult persistRecoveryCandidate(
        RunCommandState& current, const RunCommandState& candidate,
        const RunCheckpointTime& time,
        RunPersistenceFallbackDirective fallbackDirective = {});

   private:
    friend class RunPersistenceCoordinatorTestAccess;

    // `mutationKind` is explicit (6.14.2) rather than inferred from
    // commandId presence: persistTransition and persistSensorSelection both
    // omit commandId, but need distinct RunPersistenceHead::mutationKind
    // values. Ignored when `periodic` is true (a Committed head never
    // records a mutation kind).
    [[nodiscard]] RunPersistenceResult writeSnapshot(
        const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
        bool periodic, const RunCommandState& before,
        RunPersistenceMutationKind mutationKind,
        std::optional<CommandId> commandId = std::nullopt);
    [[nodiscard]] RunPersistenceResult writeSnapshotCore(
        const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
        bool periodic, const RunCommandState& before,
        RunPersistenceMutationKind mutationKind,
        std::optional<CommandId> commandId,
        std::optional<std::size_t> targetSlotOverride,
        RunPersistenceFallbackDirective fallbackDirective,
        RunPersistenceCoordinatorState rollbackState);
    [[nodiscard]] RunPersistenceResult unavailableResult() const;
    [[nodiscard]] RunPersistenceResult result(
        RunPersistenceResultStatus status,
        RunPersistenceStep step = RunPersistenceStep::None,
        RunPersistenceTechnicalReason reason =
            RunPersistenceTechnicalReason::None,
        RunPersistenceDurability durability =
            RunPersistenceDurability::Unchanged) const;
    void enterBlockedIndeterminate();

    RunPersistenceStore store_;
    device_platform::StorageEpoch epoch_;
    RunCheckpointSchedule schedule_;
    RunPersistenceCoordinatorState state_{
        RunPersistenceCoordinatorState::Uninitialized};
    std::optional<RunPersistenceRawRecord> slots_[2];
    std::optional<RunPersistenceHead> currentHead_;
    std::array<CommandId, kMaximumPersistedRunCommandIds> persistedIds_{};
    std::size_t persistedIdCount_{0U};
    std::uint64_t nextCheckpointRevision_{1U};
    std::uint64_t nextHeadRevision_{1U};
};

}  // namespace fermentation
