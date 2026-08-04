#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "run_commands.hpp"

namespace fermentation {

inline constexpr std::size_t kMaximumPersistedRunCommandIds = 32U;
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

// Deliberately only the run-domain projection. Messages, fault/safety state,
// command-session state and journal history are outside Issue #17.
struct RunPersistenceSnapshot {
    RunCheckpointVariant variant{RunCheckpointVariant::NoActiveRun};
    RunCheckpointTrigger trigger{RunCheckpointTrigger::Command};
    std::uint64_t checkpointRevision{0U};
    std::uint64_t checkpointMonotonicMillis{0U};
    std::optional<std::int64_t> checkpointUtcUnixSeconds;
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

[[nodiscard]] bool isPersistedRunCommand(CommandKind kind);
[[nodiscard]] bool validateRunPersistenceSnapshot(
    const RunPersistenceSnapshot& snapshot);

// A projection is constructed only from the canonical candidate state. The
// separate durable ID window is supplied by the coordinator and never copied
// from RunCommandState's mixed in-memory window.
[[nodiscard]] std::optional<RunPersistenceSnapshot>
makeRunPersistenceSnapshot(
    const RunCommandState& state,
    const std::array<CommandId, kMaximumPersistedRunCommandIds>& ids,
    std::size_t idCount, RunCheckpointTrigger trigger,
    std::uint64_t checkpointRevision, const RunCheckpointTime& time,
    std::uint16_t intervalMinutes);

// Technical restoration only: reconstructs exactly the run projection. It
// does not make recovery, boot, fault or safety decisions.
[[nodiscard]] std::optional<RunCommandState> restoreRunPersistenceSnapshot(
    const RunPersistenceSnapshot& snapshot);

}  // namespace fermentation
