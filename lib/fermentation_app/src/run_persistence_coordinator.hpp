#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

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
    PersistenceCommittedApplyFailed,
};

enum class RunPersistenceResultStatus : std::uint8_t {
    Applied,
    CheckpointWritten,
    AlreadyPersisted,
    NotEligible,
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
};

struct RunPersistenceLoadResult {
    RunPersistenceLoadStatus status{RunPersistenceLoadStatus::ReadFailed};
    std::optional<RunPersistenceSnapshot> snapshot;
};

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
        const RunCheckpointTime& time);
    [[nodiscard]] RunPersistenceResult persistTransition(
        RunCommandState& current, const TransitionDecision& decision,
        const RunCheckpointTime& time);
    [[nodiscard]] RunPersistenceResult checkpointPeriodic(
        const RunCommandState& current, const RunCheckpointTime& time);
    [[nodiscard]] RunPersistenceCoordinatorState state() const {
        return state_;
    }

   private:
    [[nodiscard]] RunPersistenceResult writeSnapshot(
        const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
        bool periodic);
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
