#include "run_persistence_contract.hpp"

#include <algorithm>

#include "run_limits.hpp"

namespace fermentation {
namespace {

// Recovery Fault became a valid active-run wire state with schema 3. Keep
// this feature boundary stable when a later schema extends the wire format.
constexpr std::uint32_t kActiveFaultIntroducedInSchema = 3U;

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
                case ProcessState::Fault:
                // Schema 3 (#18, 5.14 Punkt 1): ein Hop-1-only-Kandidat (die
                // Zeitfrage ist noch offen, kein Resume) ist ein gueltiger,
                // speicherbarer aktiver Zustand. Die engere Konsistenzpruefung
                // (welcher PendingRecoveryAnchor-Kontext dafuer noetig ist)
                // steht in validRecoveryFieldsForSnapshot(), nicht hier.
                case ProcessState::RecoveryEvaluation:
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
                case ProcessState::Fault:
                case ProcessState::RecoveryEvaluation:
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

// Schema 3 (#18, 5.14 Punkt 6): NoActiveRun traegt keine Recovery-
// Diagnosedaten eines beendeten Laufs - dieselben sechs Felder, die
// clearActiveRunState() zuruecksetzt (run_commands.cpp).
bool noActiveRunHasNoRecoveryFields(const RunPersistenceSnapshot& snapshot) {
    return !snapshot.pendingRecoveryAnchor.has_value() &&
           !snapshot.recoveryBootAnchorMonotonicMillis.has_value() &&
           !snapshot.lastRecoveryEpisodeEvidence.has_value() &&
           !snapshot.priorBootPhaseElapsed.has_value() &&
           !snapshot.nominalRecoveryAdjustment.has_value() &&
           !snapshot.runProgress.weightedProgress.has_value();
}

// Schema 3 (#18, 5.14 Punkt 2/3): ein gesetzter PendingRecoveryAnchor ist nur
// in genau zwei Faellen gueltig - waehrend Hop-1-only RecoveryEvaluation
// (Punkt 2) oder bei einem bereits resumten Lauf mit noch offener
// Zeitbewertung (Punkt 3, "Zeitbewertung noch offen").
bool validPendingRecoveryAnchorContext(const RunPersistenceSnapshot& snapshot) {
    if (snapshot.processState.state == ProcessState::RecoveryEvaluation) {
        if (!snapshot.pendingRecoveryAnchor.has_value() ||
            !snapshot.recoveryBootAnchorMonotonicMillis.has_value()) {
            return false;
        }
        const auto& anchor = *snapshot.pendingRecoveryAnchor;
        // Korrekturauftrag Befund 1: stateMatchesRunSnapshot() liefert fuer
        // nicht run-gebundene Zustaende (Boot/Standby/Completed/
        // RecoveryEvaluation/Fault/ServiceMode) absichtlich true und kann
        // eine echte Recovery-Altphase deshalb nicht allein pruefen -
        // stateUsesRunSnapshot() grenzt zusaetzlich auf genau die acht
        // Phasen ein, aus denen ueberhaupt ein Hop 1 stattfinden kann.
        return stateUsesRunSnapshot(anchor.originalProcessState.state) &&
               snapshot.processRunSnapshot.has_value() &&
               stateMatchesRunSnapshot(anchor.originalProcessState.state,
                                       *snapshot.processRunSnapshot);
    }
    if (!snapshot.pendingRecoveryAnchor.has_value()) {
        return true;
    }
    const auto state = snapshot.processState.state;
    if (state != ProcessState::WaitingForProduct &&
        state != ProcessState::Fermenting &&
        state != ProcessState::CoolHolding) {
        return false;
    }
    return snapshot.recoveryBootAnchorMonotonicMillis.has_value() &&
           snapshot.priorBootPhaseElapsed.has_value() &&
           snapshot.priorBootPhaseElapsed->taggedState == state &&
           !snapshot.priorBootPhaseElapsed->elapsed.upperBoundSeconds
                .has_value();
}

// Korrekturauftrag Befund 2: geordnete Ober-/Untergrenze fuer beide
// persistierten PriorBootPhaseElapsed-Vorkommen (pendingRecoveryAnchor.
// accumulatedBeforeEpisode, priorBootPhaseElapsed->elapsed). Gleichheit ist
// zulaessig (ein exakt bewiesener Einzelwert).
bool validPriorBootPhaseElapsedBounds(const PriorBootPhaseElapsed& elapsed) {
    return !elapsed.upperBoundSeconds.has_value() ||
           *elapsed.upperBoundSeconds >= elapsed.lowerBoundSeconds;
}

// Korrekturauftrag Befund 4: firstAfterRestart entsteht laut 5.20
// ausschliesslich ueber den Latch (Valid && filteredCelsius gesetzt) - ein
// gesetztes Rollenfeld mit einer anderen Qualitaet oder ohne Messwert ist
// strukturell unmoeglich und damit ungueltig. beforeOutage/lastKnown
// behalten ihre bestehende Last-known-/Quality-Semantik (jede Qualitaet
// zulaessig) und werden hier bewusst nicht geprueft.
bool validFirstAfterRestartRoleEvidence(
    const std::optional<RoleTemperatureEvidence>& role) {
    return !role.has_value() ||
           (role->quality == device_platform::SensorQuality::Valid &&
            role->filteredCelsius.has_value());
}

bool validFirstAfterRestartEvidence(const FirstAfterRestartEvidence& evidence) {
    return validFirstAfterRestartRoleEvidence(evidence.air) &&
           validFirstAfterRestartRoleEvidence(evidence.product) &&
           validFirstAfterRestartRoleEvidence(evidence.cooling);
}

// Schema 3 (#18, 5.14 Punkt 4): genau eine der beiden Tag-Bedeutungen -
// Normalfall (Tag == aktuelle Phase) oder die Hop-1-only-Ausnahme waehrend
// RecoveryEvaluation (Tag == urspruengliche Ankerphase). RecoveryEvaluation
// selbst ist nie ein gueltiger Tag-Wert.
bool validPriorBootPhaseElapsedTag(const RunPersistenceSnapshot& snapshot) {
    if (!snapshot.priorBootPhaseElapsed.has_value()) {
        return true;
    }
    const auto tag = snapshot.priorBootPhaseElapsed->taggedState;
    if (snapshot.processState.state != ProcessState::RecoveryEvaluation) {
        return tag == snapshot.processState.state;
    }
    return snapshot.pendingRecoveryAnchor.has_value() &&
           tag == snapshot.pendingRecoveryAnchor->originalProcessState.state;
}

// Korrekturauftrag Befund 3 (5.14/5.21): die Konfidenz muss zur Quelle
// passen - Product buchte einen bevorzugten Beitrag (ProductPreferred), Air
// ausschliesslich einen bereits validierten Fallback (AirReduced). Eine
// andere Kombination waere eine geratene oder inkonsistent geschriebene
// Provenienz.
bool validWeightedProgressRoleConfidence(
    const WeightedProgressProvenance& provenance) {
    switch (provenance.lastSourceRole) {
        case RunSensorMode::Product:
            return provenance.confidence ==
                   WeightedProgressConfidence::ProductPreferred;
        case RunSensorMode::Air:
            return provenance.confidence ==
                   WeightedProgressConfidence::AirReduced;
    }
    return false;
}

// Schema 3 (#18, 5.14 Punkt 7, referenziert 5.21): Coverage-/
// Provenienzinvariante des optionalen temperaturgewichteten Zustands.
bool validWeightedProgressState(const WeightedProgressState& weighted) {
    if (weighted.lastApplied.has_value() &&
        (weighted.lastApplied->modelRevision == 0U ||
         weighted.lastApplied->lastAppliedSegmentId == 0U ||
         !validWeightedProgressRoleConfidence(*weighted.lastApplied))) {
        return false;
    }
    switch (weighted.coverage) {
        case WeightedProgressCoverage::Complete:
            return weighted.lastApplied.has_value() &&
                   weighted.cumulative.upperBoundSeconds.has_value() &&
                   *weighted.cumulative.upperBoundSeconds >=
                       weighted.cumulative.lowerBoundSeconds;
        case WeightedProgressCoverage::PartialUnknown:
            return !weighted.cumulative.upperBoundSeconds.has_value() &&
                   (weighted.lastApplied.has_value() ||
                    weighted.cumulative.lowerBoundSeconds == 0U);
    }
    return false;
}

// Schema 3 (#18, 5.14): Bindeglied fuer alle Recovery-/Progressfeld-
// Invarianten dieses Abschnitts, fuer variant == NoActiveRun (Punkt 6) und
// jeden aktiven Variant (Punkte 2/3/4/7) einheitlich aufgerufen.
bool validRecoveryFieldsForSnapshot(const RunPersistenceSnapshot& snapshot) {
    if (snapshot.variant == RunCheckpointVariant::NoActiveRun) {
        return noActiveRunHasNoRecoveryFields(snapshot);
    }
    if (!validPendingRecoveryAnchorContext(snapshot) ||
        !validPriorBootPhaseElapsedTag(snapshot)) {
        return false;
    }
    if (snapshot.pendingRecoveryAnchor.has_value() &&
        !validPriorBootPhaseElapsedBounds(
            snapshot.pendingRecoveryAnchor->accumulatedBeforeEpisode)) {
        return false;
    }
    if (snapshot.priorBootPhaseElapsed.has_value() &&
        !validPriorBootPhaseElapsedBounds(
            snapshot.priorBootPhaseElapsed->elapsed)) {
        return false;
    }
    if (snapshot.lastRecoveryEpisodeEvidence.has_value()) {
        if (snapshot.lastRecoveryEpisodeEvidence->weightedProgressSegmentId
                .has_value() &&
            *snapshot.lastRecoveryEpisodeEvidence->weightedProgressSegmentId ==
                0U) {
            return false;
        }
        if (!validFirstAfterRestartEvidence(
                snapshot.lastRecoveryEpisodeEvidence->firstAfterRestart)) {
            return false;
        }
    }
    return !snapshot.runProgress.weightedProgress.has_value() ||
           validWeightedProgressState(*snapshot.runProgress.weightedProgress);
}

}  // namespace

bool knownRunPersistenceSchema(std::uint32_t schemaVersion) {
    return schemaVersion == 1U || schemaVersion == 2U ||
           schemaVersion == kCurrentRunPersistenceSchema;
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
        case CommandKind::ApplyRecoveryTimeCorrection:
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
               validRecoveryFieldsForSnapshot(snapshot) &&
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
                                         snapshot.runRevision) ||
        !validRecoveryFieldsForSnapshot(snapshot)) {
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

bool validateRunPersistenceSnapshotForSchema(
    const RunPersistenceSnapshot& snapshot, std::uint32_t schemaVersion) {
    if (!knownRunPersistenceSchema(schemaVersion)) return false;
    if (schemaVersion < kActiveFaultIntroducedInSchema &&
        snapshot.variant != RunCheckpointVariant::NoActiveRun &&
        snapshot.processState.state == ProcessState::Fault) {
        return false;
    }
    return validateRunPersistenceSnapshot(snapshot);
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
    // Schema 3 (#18, 5.14 Punkt 6): recoveryTemperatureEvidence und
    // recoveryEpisodeRevision sind keine laufgebundenen Diagnosefelder (5.20)
    // bzw. ein monotoner Zaehler wie runRevision - beide werden unbedingt
    // kopiert. Die uebrigen fuenf Recovery-/Progressfelder sind dagegen nur
    // fuer einen aktiven Run gueltig (validateRunPersistenceSnapshot lehnt sie
    // bei NoActiveRun ab) und werden deshalb ausschliesslich in den beiden
    // aktiven Zweigen unten kopiert - eine stale RAM-Anker bei NoActiveRun
    // faellt sonst erst als InvalidProjection auf, statt niemals zu
    // entstehen.
    snapshot.recoveryTemperatureEvidence = state.recoveryTemperatureEvidence;
    snapshot.recoveryEpisodeRevision = state.recoveryEpisodeRevision;
    if (hasProgram) {
        snapshot.variant = RunCheckpointVariant::ProgramRun;
        snapshot.activeRunId = state.activeRunId;
        snapshot.activeRunSensorMode = state.activeRunSensorMode;
        snapshot.sensorSelection = state.sensorSelection;
        snapshot.program = state.activeProgramRun->snapshot();
        snapshot.revisions = state.activeProgramRun->revisions();
        snapshot.revisionCount = state.activeProgramRun->revisionCount();
        snapshot.processRunSnapshot = state.processRunSnapshot;
        snapshot.pendingRecoveryAnchor = state.pendingRecoveryAnchor;
        snapshot.recoveryBootAnchorMonotonicMillis =
            state.recoveryBootAnchorMonotonicMillis;
        snapshot.lastRecoveryEpisodeEvidence =
            state.lastRecoveryEpisodeEvidence;
        snapshot.priorBootPhaseElapsed = state.priorBootPhaseElapsed;
        snapshot.nominalRecoveryAdjustment = state.nominalRecoveryAdjustment;
        snapshot.runProgress = state.runProgress;
    } else if (hasManual) {
        snapshot.variant = RunCheckpointVariant::ManualRun;
        snapshot.activeRunId = state.activeRunId;
        snapshot.activeRunSensorMode = state.activeRunSensorMode;
        snapshot.sensorSelection = state.sensorSelection;
        snapshot.manual = state.activeManualRun;
        snapshot.processRunSnapshot = state.processRunSnapshot;
        snapshot.pendingRecoveryAnchor = state.pendingRecoveryAnchor;
        snapshot.recoveryBootAnchorMonotonicMillis =
            state.recoveryBootAnchorMonotonicMillis;
        snapshot.lastRecoveryEpisodeEvidence =
            state.lastRecoveryEpisodeEvidence;
        snapshot.priorBootPhaseElapsed = state.priorBootPhaseElapsed;
        snapshot.nominalRecoveryAdjustment = state.nominalRecoveryAdjustment;
        snapshot.runProgress = state.runProgress;
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
    // Siehe Kommentar in makeRunPersistenceSnapshot: dieselbe Zweiteilung
    // rueckwaerts.
    restored.recoveryTemperatureEvidence = snapshot.recoveryTemperatureEvidence;
    restored.recoveryEpisodeRevision = snapshot.recoveryEpisodeRevision;
    if (snapshot.variant == RunCheckpointVariant::ProgramRun) {
        restored.activeProgramRun = ActiveRun::restore(
            *snapshot.program, snapshot.revisions, snapshot.revisionCount);
        restored.activeRunId = snapshot.activeRunId;
        restored.activeRunSensorMode = snapshot.activeRunSensorMode;
        restored.sensorSelection = snapshot.sensorSelection;
        restored.processRunSnapshot = snapshot.processRunSnapshot;
        restored.pendingRecoveryAnchor = snapshot.pendingRecoveryAnchor;
        restored.recoveryBootAnchorMonotonicMillis =
            snapshot.recoveryBootAnchorMonotonicMillis;
        restored.lastRecoveryEpisodeEvidence =
            snapshot.lastRecoveryEpisodeEvidence;
        restored.priorBootPhaseElapsed = snapshot.priorBootPhaseElapsed;
        restored.nominalRecoveryAdjustment = snapshot.nominalRecoveryAdjustment;
        restored.runProgress = snapshot.runProgress;
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
        restored.pendingRecoveryAnchor = snapshot.pendingRecoveryAnchor;
        restored.recoveryBootAnchorMonotonicMillis =
            snapshot.recoveryBootAnchorMonotonicMillis;
        restored.lastRecoveryEpisodeEvidence =
            snapshot.lastRecoveryEpisodeEvidence;
        restored.priorBootPhaseElapsed = snapshot.priorBootPhaseElapsed;
        restored.nominalRecoveryAdjustment = snapshot.nominalRecoveryAdjustment;
        restored.runProgress = snapshot.runProgress;
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
