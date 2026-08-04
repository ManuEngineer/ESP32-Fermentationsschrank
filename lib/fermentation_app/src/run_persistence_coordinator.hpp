#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "run_checkpoint_schedule.hpp"
#include "run_persistence_contract.hpp"
#include "state_store.hpp"
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
};

struct RunPersistenceResult {
    RunPersistenceResultStatus status{RunPersistenceResultStatus::Blocked};
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
    RunPersistenceCoordinator& operator=(const RunPersistenceCoordinator&) = delete;
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
    [[nodiscard]] RunPersistenceCoordinatorState state() const { return state_; }

    struct RecordReference {
        std::uint8_t slot{0U};
        std::uint64_t checkpointRevision{0U};
        std::uint32_t payloadLength{0U};
        std::uint32_t payloadCrc{0U};
        RunCheckpointVariant variant{RunCheckpointVariant::NoActiveRun};
    };
    struct Head {
        enum class State : std::uint8_t { Prepared = 1U, Committed = 2U };
        State state{State::Prepared};
        std::uint64_t revision{0U};
        RecordReference current;
        std::optional<RecordReference> fallback;
        std::string bytes;
    };
    struct RawRecord {
        std::string bytes;
        RunPersistenceSnapshot snapshot;
    };

   private:

    [[nodiscard]] RunPersistenceResult writeSnapshot(
        const RunPersistenceSnapshot& snapshot, bool periodic);
    [[nodiscard]] RunPersistenceResult unavailableResult() const;
    void enterBlockedIndeterminate();

    device_platform::IStateStore& store_;
    device_platform::StorageEpoch epoch_;
    RunCheckpointSchedule schedule_;
    RunPersistenceCoordinatorState state_{RunPersistenceCoordinatorState::Uninitialized};
    std::optional<RawRecord> slots_[2];
    std::optional<Head> currentHead_;
    std::array<CommandId, kMaximumPersistedRunCommandIds> persistedIds_{};
    std::size_t persistedIdCount_{0U};
    std::uint64_t nextCheckpointRevision_{1U};
    std::uint64_t nextHeadRevision_{1U};
};

}  // namespace fermentation
