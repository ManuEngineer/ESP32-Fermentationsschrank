#include "run_persistence_contract.hpp"

#include <algorithm>

#include "run_limits.hpp"

namespace fermentation {
namespace {

bool validStateFor(RunCheckpointVariant variant, ProcessState state) {
    switch (variant) {
        case RunCheckpointVariant::ProgramRun:
            switch (state) {
                case ProcessState::Preheating:
                case ProcessState::WaitingForProduct:
                case ProcessState::ReachingTarget:
                case ProcessState::QualifyingTarget:
                case ProcessState::Fermenting:
                case ProcessState::Cooling:
                case ProcessState::CoolHolding:
                case ProcessState::Completed:
                    return true;
                default:
                    return false;
            }
        case RunCheckpointVariant::ManualRun:
            switch (state) {
                case ProcessState::Preheating:
                case ProcessState::WaitingForProduct:
                case ProcessState::ReachingTarget:
                case ProcessState::QualifyingTarget:
                case ProcessState::ManualHolding:
                case ProcessState::Completed:
                    return true;
                default:
                    return false;
            }
        case RunCheckpointVariant::NoActiveRun:
            return state == ProcessState::Standby;
    }
    return false;
}

bool validIds(const RunPersistenceSnapshot& snapshot) {
    if (snapshot.persistedRunCommandCount >
        snapshot.persistedRunCommandIds.size()) {
        return false;
    }
    for (std::size_t i = 0U; i < snapshot.persistedRunCommandCount; ++i) {
        if (snapshot.persistedRunCommandIds[i] == 0U) {
            return false;
        }
        for (std::size_t j = 0U; j < i; ++j) {
            if (snapshot.persistedRunCommandIds[i] ==
                snapshot.persistedRunCommandIds[j]) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

bool isPersistedRunCommand(CommandKind kind) {
    switch (kind) {
        case CommandKind::StartProgram:
        case CommandKind::StartManualHolding:
        case CommandKind::AbortAndTurnOff:
        case CommandKind::AbortAndCool:
        case CommandKind::AcknowledgeCompletion:
        case CommandKind::CoolAfterCompletion:
        case CommandKind::AdjustRun:
            return true;
        case CommandKind::AcknowledgeMessage:
        case CommandKind::MuteMessage:
        case CommandKind::ResetFault:
            return false;
    }
    return false;
}

bool validateRunPersistenceSnapshot(const RunPersistenceSnapshot& snapshot) {
    if (snapshot.checkpointRevision == 0U ||
        snapshot.intervalMinutes < kMinimumRunCheckpointIntervalMinutes ||
        snapshot.intervalMinutes > kMaximumRunCheckpointIntervalMinutes ||
        !validIds(snapshot) || !validStateFor(snapshot.variant,
                                               snapshot.processState.state)) {
        return false;
    }
    if (snapshot.variant == RunCheckpointVariant::NoActiveRun) {
        return snapshot.activeRunId.empty() &&
               !snapshot.activeRunSensorMode.has_value() &&
               !snapshot.program.has_value() && snapshot.revisionCount == 0U &&
               !snapshot.manual.has_value() &&
               !snapshot.processRunSnapshot.has_value();
    }
    if (!run_limits::validRunId(snapshot.activeRunId) ||
        !snapshot.activeRunSensorMode.has_value() ||
        !snapshot.processRunSnapshot.has_value()) {
        return false;
    }
    if (snapshot.variant == RunCheckpointVariant::ProgramRun) {
        if (!snapshot.program.has_value() || snapshot.manual.has_value() ||
            snapshot.revisionCount > kMaximumRunRevisions ||
            snapshot.processRunSnapshot->kind != ProcessKind::Timed) {
            return false;
        }
        return ActiveRun::restore(*snapshot.program, snapshot.revisions,
                                  snapshot.revisionCount)
            .has_value();
    }
    return !snapshot.program.has_value() && snapshot.revisionCount == 0U &&
           snapshot.manual.has_value() &&
           snapshot.processRunSnapshot->kind == ProcessKind::ManualHolding &&
           snapshot.manual->values.runId == snapshot.activeRunId &&
           snapshot.manual->values.sensorMode == *snapshot.activeRunSensorMode &&
           validateManualRunPlan(*snapshot.manual);
}

std::optional<RunPersistenceSnapshot> makeRunPersistenceSnapshot(
    const RunCommandState& state,
    const std::array<CommandId, kMaximumPersistedRunCommandIds>& ids,
    std::size_t idCount, RunCheckpointTrigger trigger,
    std::uint64_t checkpointRevision, const RunCheckpointTime& time,
    std::uint16_t intervalMinutes) {
    RunPersistenceSnapshot snapshot;
    snapshot.trigger = trigger;
    snapshot.checkpointRevision = checkpointRevision;
    snapshot.checkpointMonotonicMillis = time.monotonicMillis;
    snapshot.checkpointUtcUnixSeconds = time.utcUnixSeconds;
    snapshot.intervalMinutes = intervalMinutes;
    snapshot.processState = state.processState;
    snapshot.runRevision = state.runRevision;
    snapshot.persistedRunCommandIds = ids;
    snapshot.persistedRunCommandCount = idCount;
    if (state.activeProgramRun.has_value()) {
        snapshot.variant = RunCheckpointVariant::ProgramRun;
        snapshot.activeRunId = state.activeRunId;
        snapshot.activeRunSensorMode = state.activeRunSensorMode;
        snapshot.program = state.activeProgramRun->snapshot();
        snapshot.revisions = state.activeProgramRun->revisions();
        snapshot.revisionCount = state.activeProgramRun->revisionCount();
        snapshot.processRunSnapshot = state.processRunSnapshot;
    } else if (state.activeManualRun.has_value()) {
        snapshot.variant = RunCheckpointVariant::ManualRun;
        snapshot.activeRunId = state.activeRunId;
        snapshot.activeRunSensorMode = state.activeRunSensorMode;
        snapshot.manual = state.activeManualRun;
        snapshot.processRunSnapshot = state.processRunSnapshot;
    } else {
        snapshot.variant = RunCheckpointVariant::NoActiveRun;
    }
    return validateRunPersistenceSnapshot(snapshot)
               ? std::optional<RunPersistenceSnapshot>{std::move(snapshot)}
               : std::nullopt;
}

std::optional<RunCommandState> restoreRunPersistenceSnapshot(
    const RunPersistenceSnapshot& snapshot) {
    if (!validateRunPersistenceSnapshot(snapshot)) {
        return std::nullopt;
    }
    RunCommandState restored;
    restored.processState = snapshot.processState;
    restored.runRevision = snapshot.runRevision;
    if (snapshot.variant == RunCheckpointVariant::ProgramRun) {
        restored.activeProgramRun = ActiveRun::restore(
            *snapshot.program, snapshot.revisions, snapshot.revisionCount);
        restored.activeRunId = snapshot.activeRunId;
        restored.activeRunSensorMode = snapshot.activeRunSensorMode;
        restored.processRunSnapshot = snapshot.processRunSnapshot;
    } else if (snapshot.variant == RunCheckpointVariant::ManualRun) {
        restored.activeManualRun = snapshot.manual;
        restored.activeRunId = snapshot.activeRunId;
        restored.activeRunSensorMode = snapshot.activeRunSensorMode;
        restored.processRunSnapshot = snapshot.processRunSnapshot;
    }
    return restored;
}

}  // namespace fermentation
