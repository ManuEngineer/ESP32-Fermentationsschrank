#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "run_commands.hpp"

namespace fermentation {

inline constexpr std::size_t kMaximumPersistedRunCommandIds = 32U;
inline constexpr std::size_t kMaximumRunPersistencePayloadBytes = 8192U;
inline constexpr std::uint16_t kDefaultRunCheckpointIntervalMinutes = 5U;
inline constexpr std::uint16_t kMinimumRunCheckpointIntervalMinutes = 1U;
inline constexpr std::uint16_t kMaximumRunCheckpointIntervalMinutes = 60U;

enum class RunCheckpointVariant : std::uint8_t {
    ProgramRun = 1U,
    ManualRun = 2U,
    NoActiveRun = 3U,
};

enum class RunCheckpointTrigger : std::uint8_t {
    Command = 1U,
    Transition = 2U,
    Periodic = 3U,
};

struct RunCheckpointTime {
    std::uint64_t monotonicMillis{0U};
    std::optional<std::int64_t> utcUnixSeconds;
};

// Technical records shared by the persistence codec and store protocol. They
// deliberately contain no application/coordinator lifetime state.
struct RunCheckpointReference {
    std::uint8_t slot{0U};
    std::uint32_t schemaVersion{0U};
    std::uint64_t storageEpoch{0U};
    std::uint64_t checkpointRevision{0U};
    std::uint32_t payloadLength{0U};
    std::uint32_t payloadCrc{0U};
    RunCheckpointVariant variant{RunCheckpointVariant::NoActiveRun};
};

enum class RunPersistenceHeadState : std::uint8_t {
    Prepared = 1U,
    Committed = 2U,
};

enum class RunPersistenceMutationKind : std::uint8_t {
    Command = 1U,
    Transition = 2U,
};

struct RunPersistenceHead {
    RunPersistenceHeadState state{RunPersistenceHeadState::Prepared};
    std::uint64_t revision{0U};
    RunCheckpointReference current;
    std::optional<RunCheckpointReference> fallback;
    std::optional<RunCheckpointReference> preparedCurrent;
    std::optional<RunCheckpointReference> preparedFallback;
    RunCheckpointReference target;
    RunPersistenceMutationKind mutationKind{
        RunPersistenceMutationKind::Command};
    std::optional<CommandId> commandId;
    std::uint32_t oldRunRevision{0U};
    std::uint32_t newRunRevision{0U};
    std::uint32_t oldTransitionSequence{0U};
    std::uint32_t newTransitionSequence{0U};
    std::string bytes;
};

// Deliberately only the run-domain projection. Messages, fault/safety state,
// command-session state and journal history are outside Issue #17.
struct RunPersistenceSnapshot {
    RunCheckpointVariant variant{RunCheckpointVariant::NoActiveRun};
    RunCheckpointTrigger trigger{RunCheckpointTrigger::Command};
    std::uint64_t checkpointMonotonicMillis{0U};
    std::uint16_t intervalMinutes{kDefaultRunCheckpointIntervalMinutes};
    std::string activeRunId;
    std::optional<RunSensorMode> activeRunSensorMode;
    std::optional<RunProgramSnapshot> program;
    std::array<RunRevision, kMaximumRunRevisions> revisions{};
    std::size_t revisionCount{0U};
    std::optional<ManualRunPlan> manual;
    std::optional<ProcessRunSnapshot> processRunSnapshot;
    ProcessRuntimeState processState;
    std::uint32_t runRevision{0U};
    std::array<CommandId, kMaximumPersistedRunCommandIds>
        persistedRunCommandIds{};
    std::size_t persistedRunCommandCount{0U};
};

struct RunPersistenceRawRecord {
    std::string bytes;
    RunPersistenceSnapshot snapshot;
    std::uint64_t checkpointRevision{0U};
    std::optional<std::int64_t> utcUnixSeconds;
};

[[nodiscard]] bool isPersistedRunCommand(CommandKind kind);
[[nodiscard]] bool validateRunPersistenceSnapshot(
    const RunPersistenceSnapshot& snapshot);

// A projection is constructed only from the canonical candidate state. The
// separate durable ID window is supplied by the coordinator and never copied
// from RunCommandState's mixed in-memory window.
[[nodiscard]] std::optional<RunPersistenceSnapshot> makeRunPersistenceSnapshot(
    const RunCommandState& state,
    const std::array<CommandId, kMaximumPersistedRunCommandIds>& ids,
    std::size_t idCount, RunCheckpointTrigger trigger,
    const RunCheckpointTime& time, std::uint16_t intervalMinutes);

// Technical restoration only: reconstructs exactly the run projection. It
// does not make recovery, boot, fault or safety decisions.
[[nodiscard]] std::optional<RunCommandState> restoreRunPersistenceSnapshot(
    const RunPersistenceSnapshot& snapshot);

}  // namespace fermentation
