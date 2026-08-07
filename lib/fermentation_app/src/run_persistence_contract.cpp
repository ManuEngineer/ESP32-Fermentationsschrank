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

// Korrekturauftrag Befund 4 (6.12.1): lastDecisionCause == None genau mit
// Revision 0; die Entscheidungsrevision ist niemals neuer als die
// Laufrevision selbst; FallbackActive/ReturnedToProduct binden den aktiven
// Modus verbindlich, InitialSelection und LegacyUnknown (Schema-1-Restores,
// siehe run_persistence_codec.cpp) schraenken den Modus bewusst nicht ein.
bool validSensorSelectionCrossFields(
    const PersistedSensorSelectionState& persisted, RunSensorMode activeMode,
    std::uint32_t runRevision) {
    if ((persisted.lastDecisionCause == SensorSelectionDecisionCause::None) !=
        (persisted.lastDecisionRunRevision == 0U)) {
        return false;
    }
    if (persisted.lastDecisionRunRevision > runRevision) {
        return false;
    }
    if (persisted.provenance == SensorSelectionProvenance::FallbackActive &&
        activeMode != RunSensorMode::Air) {
        return false;
    }
    if (persisted.provenance == SensorSelectionProvenance::ReturnedToProduct &&
        activeMode != RunSensorMode::Product) {
        return false;
    }
    return true;
}

bool equalProcessRunSnapshot(const ProcessRunSnapshot& left,
                             const ProcessRunSnapshot& right) {
    return left.kind == right.kind &&
           left.preheatEnabled == right.preheatEnabled &&
           left.completionMode == right.completionMode &&
           left.qualificationDurationMinutes ==
               right.qualificationDurationMinutes &&
           left.maximumTargetReachMinutes == right.maximumTargetReachMinutes &&
           left.maximumProductWaitMinutes == right.maximumProductWaitMinutes &&
           left.fermentationDurationMinutes ==
               right.fermentationDurationMinutes &&
           left.holdDurationMinutes == right.holdDurationMinutes;
}

}  // namespace

bool knownRunPersistenceSchema(std::uint32_t schemaVersion) {
    return schemaVersion == 1U || schemaVersion == kCurrentRunPersistenceSchema;
}

bool isPersistedRunCommand(CommandKind kind) {
    switch (kind) {
        case CommandKind::StartProgram:
        case CommandKind::StartManualHolding:
        case CommandKind::AbortAndTurnOff:
        case CommandKind::AbortAndCool:
        case CommandKind::AcknowledgeCompletion:
        case CommandKind::CoolAfterCompletion:
        case CommandKind::AdjustRun:
        // #21, 6.14.1: analog zu allen anderen laufwirksamen Kommandos.
        case CommandKind::ApplySensorSelectionAction:
            return true;
        case CommandKind::AcknowledgeMessage:
        case CommandKind::MuteMessage:
        case CommandKind::ResetFault:
            return false;
    }
    return false;
}

bool validateRunPersistenceSnapshot(const RunPersistenceSnapshot& snapshot) {
    if (snapshot.intervalMinutes < kMinimumRunCheckpointIntervalMinutes ||
        snapshot.intervalMinutes > kMaximumRunCheckpointIntervalMinutes ||
        !validIds(snapshot) ||
        !validStateFor(snapshot.variant, snapshot.processState.state)) {
        return false;
    }
    if (snapshot.variant == RunCheckpointVariant::NoActiveRun) {
        return snapshot.activeRunId.empty() &&
               !snapshot.activeRunSensorMode.has_value() &&
               !snapshot.sensorSelection.has_value() &&
               !snapshot.program.has_value() && snapshot.revisionCount == 0U &&
               !snapshot.manual.has_value() &&
               !snapshot.processRunSnapshot.has_value() &&
               validateProcessRuntimeForCheckpoint(
                   snapshot.processState, nullptr,
                   snapshot.checkpointMonotonicMillis);
    }
    // Korrekturauftrag Befund 4: jeder aktive Snapshot verlangt sensorSelection
    // unbedingt - nicht mehr nur schema-2-abhaengig. Ein Schema-1-Restore wird
    // seit der Codec-Korrektur immer auf LegacyUnknown/None/0 abgebildet
    // (run_persistence_codec.cpp), traegt das Feld also ebenfalls.
    if (!run_limits::validRunId(snapshot.activeRunId) ||
        !snapshot.activeRunSensorMode.has_value() ||
        !snapshot.processRunSnapshot.has_value() ||
        !snapshot.sensorSelection.has_value() ||
        !validSensorSelectionCrossFields(*snapshot.sensorSelection,
                                         *snapshot.activeRunSensorMode,
                                         snapshot.runRevision)) {
        return false;
    }
    if (snapshot.variant == RunCheckpointVariant::ProgramRun) {
        if (!snapshot.program.has_value() || snapshot.manual.has_value() ||
            snapshot.revisionCount > kMaximumRunRevisions ||
            snapshot.processRunSnapshot->kind != ProcessKind::Timed) {
            return false;
        }
        const auto restored = ActiveRun::restore(
            *snapshot.program, snapshot.revisions, snapshot.revisionCount);
        const auto expectedProcess = restored.has_value()
                                         ? makeProcessRunSnapshot(*restored)
                                         : std::optional<ProcessRunSnapshot>{};
        return restored.has_value() && expectedProcess.has_value() &&
               validateProcessRuntimeForCheckpoint(
                   snapshot.processState, &*snapshot.processRunSnapshot,
                   snapshot.checkpointMonotonicMillis) &&
               equalProcessRunSnapshot(*expectedProcess,
                                       *snapshot.processRunSnapshot);
    }
    const auto expectedProcess = snapshot.manual.has_value()
                                     ? makeProcessRunSnapshot(*snapshot.manual)
                                     : std::optional<ProcessRunSnapshot>{};
    return !snapshot.program.has_value() && snapshot.revisionCount == 0U &&
           snapshot.manual.has_value() && expectedProcess.has_value() &&
           snapshot.processRunSnapshot->kind == ProcessKind::ManualHolding &&
           snapshot.manual->values.runId == snapshot.activeRunId &&
           snapshot.manual->values.sensorMode ==
               *snapshot.activeRunSensorMode &&
           validateManualRunPlan(*snapshot.manual) &&
           equalProcessRunSnapshot(*expectedProcess,
                                   *snapshot.processRunSnapshot) &&
           validateProcessRuntimeForCheckpoint(
               snapshot.processState, &*snapshot.processRunSnapshot,
               snapshot.checkpointMonotonicMillis);
}

std::optional<RunPersistenceSnapshot> makeRunPersistenceSnapshot(
    const RunCommandState& state,
    const std::array<CommandId, kMaximumPersistedRunCommandIds>& ids,
    std::size_t idCount, RunCheckpointTrigger trigger,
    const RunCheckpointTime& time, std::uint16_t intervalMinutes) {
    const bool hasProgram = state.activeProgramRun.has_value();
    const bool hasManual = state.activeManualRun.has_value();
    if (hasProgram && hasManual) {
        return std::nullopt;
    }
    if (!hasProgram && !hasManual &&
        (!state.activeRunId.empty() || state.activeRunSensorMode.has_value() ||
         state.processRunSnapshot.has_value())) {
        return std::nullopt;
    }
    // #21, 6.12: validateRunPersistenceSnapshot now enforces the mandatory-
    // presence rule unconditionally (Korrekturauftrag Befund 4 - a schema-1
    // restore is mapped onto LegacyUnknown/None/0 by the codec, not left
    // absent, so the schema-agnostic validator can require presence for
    // every active-run variant). This early check stays as a defensive,
    // cheaper reject at the write boundary; all start paths
    // (decideProgramStart, decideManualStart, incl. the
    // AbortAndCool/CoolAfterCompletion cooling- replacement runs) populate
    // sensorSelection unconditionally, so this can never legitimately fail for
    // a freshly-built candidate.
    if ((hasProgram || hasManual) && !state.sensorSelection.has_value()) {
        return std::nullopt;
    }
    RunPersistenceSnapshot snapshot;
    snapshot.trigger = trigger;
    snapshot.checkpointMonotonicMillis = time.monotonicMillis;
    snapshot.intervalMinutes = intervalMinutes;
    snapshot.processState = state.processState;
    snapshot.runRevision = state.runRevision;
    snapshot.persistedRunCommandIds = ids;
    snapshot.persistedRunCommandCount = idCount;
    if (hasProgram) {
        snapshot.variant = RunCheckpointVariant::ProgramRun;
        snapshot.activeRunId = state.activeRunId;
        snapshot.activeRunSensorMode = state.activeRunSensorMode;
        snapshot.sensorSelection = state.sensorSelection;
        snapshot.program = state.activeProgramRun->snapshot();
        snapshot.revisions = state.activeProgramRun->revisions();
        snapshot.revisionCount = state.activeProgramRun->revisionCount();
        snapshot.processRunSnapshot = state.processRunSnapshot;
    } else if (hasManual) {
        snapshot.variant = RunCheckpointVariant::ManualRun;
        snapshot.activeRunId = state.activeRunId;
        snapshot.activeRunSensorMode = state.activeRunSensorMode;
        snapshot.sensorSelection = state.sensorSelection;
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
        restored.sensorSelection = snapshot.sensorSelection;
        restored.processRunSnapshot = snapshot.processRunSnapshot;
        // #21, 6.12: restore uebernimmt die persistierte Auswahl, aber der
        // RAM-only-Laufzeitzustand ist fail-closed - kein Wireformat traegt
        // ihn, und bootlokale Timer (fallbackWaitStartedAtMonotonicMillis,
        // returnValidation) sind ueber einen Boot hinweg nicht gueltig.
        // #18 (Reaktivierung LoadedActiveRun -> Ready) muss diesen Zustand
        // explizit neu bewerten (computeRestartSensorSelection), bevor eine
        // Peltier-Freigabe moeglich ist.
        restored.sensorSelectionRuntime = SensorSelectionRuntimeState{};
        restored.sensorSelectionRuntime.phase =
            SensorSelectionPhase::RestartRevalidationPending;
        restored.sensorSelectionRuntime.permission =
            SensorPeltierPermission::Blocked;
    } else if (snapshot.variant == RunCheckpointVariant::ManualRun) {
        restored.activeManualRun = snapshot.manual;
        restored.activeRunId = snapshot.activeRunId;
        restored.activeRunSensorMode = snapshot.activeRunSensorMode;
        restored.sensorSelection = snapshot.sensorSelection;
        restored.processRunSnapshot = snapshot.processRunSnapshot;
        // Siehe Kommentar im ProgramRun-Zweig.
        restored.sensorSelectionRuntime = SensorSelectionRuntimeState{};
        restored.sensorSelectionRuntime.phase =
            SensorSelectionPhase::RestartRevalidationPending;
        restored.sensorSelectionRuntime.permission =
            SensorPeltierPermission::Blocked;
    }
    // NoActiveRun: restored.sensorSelectionRuntime bleibt der
    // Default-Inaktivzustand (Phase NoActiveRun, Blocked) - der explizite
    // NoActiveRun-Default aus 6.12.
    return restored;
}

}  // namespace fermentation
