#include "run_commands.hpp"

#include <cmath>
#include <limits>
#include <utility>

#include "program_limits.hpp"
#include "run_progress_weighting.hpp"
#include "run_limits.hpp"
#include "control_context.hpp"
#include "sensor_selection.hpp"

namespace fermentation {

bool foldObservedRunSeconds(RunCommandState& candidate,
                            std::uint64_t deltaSeconds) {
    if (deltaSeconds > std::numeric_limits<std::uint32_t>::max() -
                           candidate.runProgress.observedRunSeconds) {
        return false;
    }
    candidate.runProgress.observedRunSeconds +=
        static_cast<std::uint32_t>(deltaSeconds);
    return true;
}

std::optional<std::uint32_t> deriveFermentingSecondsDelta(
    const RunCommandState& before, std::uint64_t atMillis) {
    if (before.processState.state != ProcessState::Fermenting ||
        atMillis < before.processState.stateEnteredAtMillis) {
        return std::nullopt;
    }
    const auto seconds =
        (atMillis - before.processState.stateEnteredAtMillis) / 1000U;
    if (seconds > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(seconds);
}

std::optional<PriorBootPhaseElapsed> effectivePriorElapsedForFermenting(
    const RunCommandState& current) {
    if (current.processState.state != ProcessState::Fermenting) {
        return std::nullopt;
    }

    PriorBootPhaseElapsed effective;
    if (current.priorBootPhaseElapsed.has_value()) {
        if (current.priorBootPhaseElapsed->taggedState !=
            ProcessState::Fermenting) {
            return std::nullopt;
        }
        effective = current.priorBootPhaseElapsed->elapsed;
    }

    if (current.nominalRecoveryAdjustment.has_value()) {
        const auto nominal =
            current.nominalRecoveryAdjustment->cumulativeAppliedSeconds;
        const auto lower =
            static_cast<std::uint64_t>(effective.lowerBoundSeconds) + nominal;
        if (lower > std::numeric_limits<std::uint32_t>::max() ||
            (effective.upperBoundSeconds.has_value() &&
             lower > *effective.upperBoundSeconds)) {
            return std::nullopt;
        }
        effective.lowerBoundSeconds = static_cast<std::uint32_t>(lower);
    }
    return effective;
}

namespace {

bool validCommandSource(CommandSource source) {
    switch (source) {
        case CommandSource::LocalDisplay:
        case CommandSource::WebInterface:
            return true;
    }
    return false;
}

bool validSensorMode(RunSensorMode mode) {
    switch (mode) {
        case RunSensorMode::Product:
        case RunSensorMode::Air:
            return true;
    }
    return false;
}

std::optional<CommittedControlContextTransition> targetContextTransition(
    bool targetChanged) {
    if (!targetChanged) {
        return std::nullopt;
    }
    return CommittedControlContextTransition::TargetContextChange;
}

bool activeProcessState(ProcessState state) {
    switch (state) {
        case ProcessState::Preheating:
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::Cooling:
        case ProcessState::CoolHolding:
        case ProcessState::ManualHolding:
            return true;
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return false;
    }
    return false;
}

bool isRunComfortCommand(CommandKind kind) {
    switch (kind) {
        case CommandKind::StartProgram:
        case CommandKind::StartManualHolding:
        case CommandKind::AbortAndTurnOff:
        case CommandKind::AbortAndCool:
        case CommandKind::AcknowledgeCompletion:
        case CommandKind::CoolAfterCompletion:
        case CommandKind::AdjustRun:
        case CommandKind::ApplyRecoveryTimeCorrection:
            return true;
        case CommandKind::AcknowledgeMessage:
        case CommandKind::MuteMessage:
        case CommandKind::ResetFault:
        // #21, 6.14.1: false, damit der generische criticalSafetyEventPending-
        // Gate in beginDecision() die Aktion nicht pauschal vor der
        // aktionsspezifischen Pruefung verwirft. Kein Safety-Bypass -
        // decideApplySensorSelectionAction prueft denselben Zustand intern
        // selbst (siehe dort).
        case CommandKind::ApplySensorSelectionAction:
            return false;
    }
    return true;
}

bool adjustmentAllowedIn(ProcessState state) {
    return state == ProcessState::Preheating ||
           state == ProcessState::ReachingTarget ||
           state == ProcessState::QualifyingTarget ||
           state == ProcessState::Fermenting;
}

// `ActiveRun` kennt `ProcessState` bewusst nicht (siehe run_snapshot.hpp);
// die Kommandoschicht bildet den aktuellen Zustand auf den schmalen
// Phasenkontext ab. Nur innerhalb von `adjustmentAllowedIn()` aufgerufen,
// daher ist `Fermenting` die einzige verbleibende Alternative zu den drei
// Vor-Fermentationszustaenden.
RunAdjustmentPhaseContext phaseContextFor(ProcessState state) {
    return state == ProcessState::Fermenting
               ? RunAdjustmentPhaseContext::Fermenting
               : RunAdjustmentPhaseContext::BeforeFermentation;
}

bool containsProcessedCommand(const RunCommandState& state, CommandId id) {
    for (std::size_t index = 0U; index < state.processedCommandCount; ++index) {
        if (state.processedCommandIds[index] == id) {
            return true;
        }
    }
    return false;
}

void rememberProcessedCommand(RunCommandState& state, CommandId id) {
    if (state.processedCommandCount < state.processedCommandIds.size()) {
        state.processedCommandIds[state.processedCommandCount] = id;
        ++state.processedCommandCount;
        return;
    }
    for (std::size_t index = 1U; index < state.processedCommandIds.size();
         ++index) {
        state.processedCommandIds[index - 1U] =
            state.processedCommandIds[index];
    }
    state.processedCommandIds.back() = id;
}

CommandDecision result(const RunCommandState& current,
                       const CommandEnvelope& envelope, CommandKind kind,
                       CommandStatus status) {
    CommandDecision decision;
    decision.status = status;
    decision.kind = kind;
    decision.envelope = envelope;
    decision.before = current;
    decision.after = current;
    return decision;
}

CommandDecision beginDecision(const RunCommandState& current,
                              const CommandEnvelope& envelope,
                              CommandKind kind) {
    auto decision = result(current, envelope, kind, CommandStatus::Proposed);
    if (envelope.id == 0U || !validCommandSource(envelope.source)) {
        decision.status = CommandStatus::InvalidInput;
    } else if (containsProcessedCommand(current, envelope.id)) {
        decision.status = CommandStatus::AlreadyProcessed;
    } else if (current.commandSequence ==
               std::numeric_limits<std::uint32_t>::max()) {
        decision.status = CommandStatus::CapacityReached;
    } else if (envelope.expectedStateSequence !=
                   current.processState.transitionSequence ||
               (current.processedCommandCount > 0U &&
                envelope.monotonicMillis <
                    current.lastCommandMonotonicMillis)) {
        decision.status = CommandStatus::StaleState;
    } else if (current.criticalSafetyEventPending &&
               isRunComfortCommand(kind)) {
        decision.status = CommandStatus::SafetyRejected;
    }
    return decision;
}

// Die Kommandosequenz gehoert zur tatsaechlich vorgeschlagenen Gesamtmutation.
// Sie wird deshalb erst nach allen nicht mutierenden Fachpruefungen unmittelbar
// an der Commit-Grenze erhoeht. Abgelehnte Entscheidungen behalten damit einen
// zu `before` identischen `after`-Zustand.
void beginMutation(CommandDecision& decision) {
    decision.after.commandSequence = decision.before.commandSequence + 1U;
    rememberProcessedCommand(decision.after, decision.envelope.id);
    decision.after.lastCommandMonotonicMillis =
        decision.envelope.monotonicMillis;
}

bool requireRunRevision(CommandDecision& decision) {
    if (!decision.proposed()) {
        return false;
    }
    if (!decision.envelope.expectedRunRevision.has_value()) {
        decision.status = CommandStatus::ContextMissing;
        return false;
    }
    if (*decision.envelope.expectedRunRevision != decision.before.runRevision) {
        decision.status = CommandStatus::StaleState;
        return false;
    }
    return true;
}

bool requireRecoveryEpisodeRevision(CommandDecision& decision) {
    if (!decision.proposed()) {
        return false;
    }
    if (!decision.envelope.expectedRecoveryEpisodeRevision.has_value()) {
        decision.status = CommandStatus::ContextMissing;
        return false;
    }
    if (*decision.envelope.expectedRecoveryEpisodeRevision !=
        decision.before.recoveryEpisodeRevision) {
        decision.status = CommandStatus::StaleState;
        return false;
    }
    return true;
}

bool requireMessageRevision(CommandDecision& decision) {
    if (!decision.proposed()) {
        return false;
    }
    if (!decision.envelope.expectedMessageRevision.has_value()) {
        decision.status = CommandStatus::ContextMissing;
        return false;
    }
    if (*decision.envelope.expectedMessageRevision !=
        decision.before.messageRevision) {
        decision.status = CommandStatus::StaleState;
        return false;
    }
    return true;
}

bool requireFaultRevision(CommandDecision& decision) {
    if (!decision.proposed()) {
        return false;
    }
    if (!decision.envelope.expectedFaultRevision.has_value()) {
        decision.status = CommandStatus::ContextMissing;
        return false;
    }
    // D1: the expected revision names the target fault's own revision, not
    // the shared FaultCore revision counter, which another fault may
    // advance independently of this target. The target-specific staleness
    // check happens once the target record is resolved, below.
    return true;
}

// Zentrale Ueberlaufpruefung fuer jeden Konflikt- oder Fachrevisionszaehler
// (runRevision, messageRevision, RuntimeMessage::revision, faultRevision).
// Muss vor jeder Erhoehung und vor jeder anderen Mutation von `decision.after`
// aufgerufen werden, damit ein bereits an `UINT32_MAX` stehender Zaehler
// `CapacityReached` liefert, ohne eine teilweise erzeugte Entscheidung, einen
// veraenderten `after`-Zustand oder einen Effect-Eintrag zu hinterlassen.
bool requireRevisionCapacity(CommandDecision& decision,
                             std::uint32_t revision) {
    if (!decision.proposed()) {
        return false;
    }
    if (revision == std::numeric_limits<std::uint32_t>::max()) {
        decision.status = CommandStatus::CapacityReached;
        return false;
    }
    return true;
}

bool addEffect(CommandDecision& decision, CommandEffect effect) {
    if (decision.effectCount >= decision.effects.size()) {
        decision.status = CommandStatus::CapacityReached;
        return false;
    }
    decision.effects[decision.effectCount] = effect;
    ++decision.effectCount;
    return true;
}

bool applyTransition(RunCommandState& state, ProcessEvent event,
                     std::uint64_t monotonicMillis,
                     const ProcessRunSnapshot* snapshot) {
    const auto transition =
        decideProcessTransition(state.processState, snapshot, {},
                                {event, std::nullopt}, monotonicMillis);
    return transition.proposed() &&
           applyProcessTransition(state.processState, transition, snapshot);
}

RunChangeSource changeSource(CommandSource source) {
    return source == CommandSource::WebInterface
               ? RunChangeSource::WebInterface
               : RunChangeSource::LocalDisplay;
}

std::optional<ManualRunPlan> makeManualPlan(const ManualRunPlanRequest& request,
                                            const CommandEnvelope& envelope) {
    ManualRunPlan plan{request, envelope.source, envelope.monotonicMillis,
                       ProcessKind::ManualHolding};
    return validateManualRunPlan(plan)
               ? std::optional<ManualRunPlan>{std::move(plan)}
               : std::nullopt;
}

bool installManualRun(RunCommandState& state, const CommandEnvelope& envelope,
                      ManualRunPlan plan) {
    const auto snapshot = makeProcessRunSnapshot(plan);
    if (!snapshot.has_value() ||
        !applyTransition(state, ProcessEvent::StartRun,
                         envelope.monotonicMillis, &*snapshot)) {
        return false;
    }
    state.activeProgramRun.reset();
    state.activeRunId = plan.values.runId;
    state.activeRunSensorMode = plan.values.sensorMode;
    state.activeManualRun = std::move(plan);
    state.processRunSnapshot = *snapshot;
    return true;
}

RuntimeMessage* findMessage(RunCommandState& state, std::uint32_t id) {
    for (std::size_t index = 0U; index < state.messageCount; ++index) {
        if (state.messages[index].id == id) {
            return &state.messages[index];
        }
    }
    return nullptr;
}

CommandStatus mapAdjustmentStatus(RunAdjustmentStatus status) {
    switch (status) {
        case RunAdjustmentStatus::Proposed:
            return CommandStatus::Proposed;
        case RunAdjustmentStatus::NotConfirmed:
            return CommandStatus::NotConfirmed;
        case RunAdjustmentStatus::NoChange:
            return CommandStatus::NoChange;
        case RunAdjustmentStatus::RunInactive:
        case RunAdjustmentStatus::InvalidStage:
        case RunAdjustmentStatus::CompletedStage:
            return CommandStatus::NotAllowedInState;
        case RunAdjustmentStatus::SafetyRejected:
            return CommandStatus::SafetyRejected;
        case RunAdjustmentStatus::InvalidValue:
        case RunAdjustmentStatus::InvalidMetadata:
        case RunAdjustmentStatus::TimestampWentBackwards:
            return CommandStatus::InvalidInput;
        case RunAdjustmentStatus::RevisionCapacityReached:
            return CommandStatus::CapacityReached;
        case RunAdjustmentStatus::Applied:
            return CommandStatus::InvalidInput;
    }
    return CommandStatus::InvalidInput;
}

// #21, 6.8: fuer einen Programmlauf direkt aus dem vertrauenswuerdigen
// Startvertrag (ProgramDocument) abgeleitet. Ein produktgefuehrter manueller
// Lauf hat keine eigene Policy-/Ruecklaufkonfiguration und folgt deshalb
// demselben WaitForUser-Ablauf wie ein entsprechend konfigurierter
// Programmlauf (6.8) - ManualReturnToProduct, da es keine automatisch
// validierte Rueckkehr ohne Programmkonfiguration gibt.
SensorSelectionProgramContext sensorSelectionProgramContextFor(
    const RunCommandState& state) {
    if (state.activeProgramRun.has_value()) {
        const auto& program =
            state.activeProgramRun->snapshot().sourceProgram.program;
        SensorSelectionProgramContext context;
        context.sensorPreference = program.sensorPreference;
        context.policy = program.productSensorFailure.policy;
        context.returnStrategy = program.productSensorFailure.returnStrategy;
        context.fallbackDelaySeconds =
            program.productSensorFailure.fallbackDelaySeconds;
        return context;
    }
    SensorSelectionProgramContext context;
    context.sensorPreference = SensorPreference::ProductIfAvailableElseAir;
    context.policy = ProductSensorFailurePolicy::WaitForUser;
    context.returnStrategy = ReturnStrategy::ManualReturnToProduct;
    context.fallbackDelaySeconds = std::nullopt;
    return context;
}

// #21, 6.14.3: Der manuelle Transport liest ausschliesslich
// CommandDecision::sensorSelectionApplyStatus fuer die Detailursache; dieser
// generische CommandStatus dient nur der bestehenden dispatcherweiten
// proposed()-Konvention (persistCommand/applyRunCommand). Wiederverwendung
// bestehender Werte statt neuer CommandStatus-Auspraegungen: StaleDecision
// und CapacityReached haben bereits ein exaktes Gegenstueck; InvalidDecision/
// InvalidContext/TimeWentBackwards teilen sich InvalidInput als bestehenden
// generischen Ablehnungswert (analog zum Muster in mapAdjustmentStatus).
// AppliedPersistentCandidate/AppliedRamOnly/NoChange sind hier nur wegen
// Exhaustivitaet gelistet - der Aufrufer (siehe Switch bei
// applySensorSelectionDecision-Auswertung) behandelt diese drei Erfolgsfaelle
// bereits vor dem Aufruf dieser Funktion und ruft sie fuer sie nie auf.
CommandStatus mapSensorSelectionRejection(SensorSelectionApplyStatus status) {
    switch (status) {
        case SensorSelectionApplyStatus::StaleDecision:
            return CommandStatus::StaleState;
        case SensorSelectionApplyStatus::CapacityReached:
            return CommandStatus::CapacityReached;
        case SensorSelectionApplyStatus::InvalidDecision:
        case SensorSelectionApplyStatus::InvalidContext:
        case SensorSelectionApplyStatus::TimeWentBackwards:
        case SensorSelectionApplyStatus::AppliedPersistentCandidate:
        case SensorSelectionApplyStatus::AppliedRamOnly:
        case SensorSelectionApplyStatus::NoChange:
            return CommandStatus::InvalidInput;
    }
    return CommandStatus::InvalidInput;
}

// #21, 6.5: vollstaendige Startmatrix. `valid=false` bedeutet Ablehnung
// (InvalidInput); `substituted=true` markiert genau Zeile 2 (angefordertes
// Produkt bei ProductIfAvailableElseAir, aber zum Startzeitpunkt nicht
// gueltig - automatischer Ersatz auf Luft). Die Luft-/Kuehlkoerper-
// Vorbedingung (Zeile-uebergreifend) wird vom Aufrufer separat geprueft.
struct ProgramStartSensorResolution {
    bool valid{false};
    RunSensorMode effectiveMode{RunSensorMode::Air};
    bool substituted{false};
};

ProgramStartSensorResolution resolveProgramStartSensorMode(
    SensorPreference preference, RunSensorMode requestedMode,
    bool productSensorValid) {
    switch (preference) {
        case SensorPreference::ProductIfAvailableElseAir:
            if (requestedMode == RunSensorMode::Air) {
                return {true, RunSensorMode::Air, false};
            }
            return productSensorValid
                       ? ProgramStartSensorResolution{true,
                                                      RunSensorMode::Product,
                                                      false}
                       : ProgramStartSensorResolution{true, RunSensorMode::Air,
                                                      true};
        case SensorPreference::AirProductOptional:
            if (requestedMode == RunSensorMode::Air) {
                return {true, RunSensorMode::Air, false};
            }
            return {productSensorValid, RunSensorMode::Product, false};
        case SensorPreference::ProductRequired:
            if (requestedMode == RunSensorMode::Air) {
                return {false, RunSensorMode::Air, false};
            }
            return {productSensorValid, RunSensorMode::Product, false};
        case SensorPreference::AirOnly:
            if (requestedMode == RunSensorMode::Product) {
                return {false, RunSensorMode::Product, false};
            }
            return {true, RunSensorMode::Air, false};
    }
    return {false, RunSensorMode::Air, false};
}

struct StartSensorSelectionOutcome {
    SensorSelectionRuntimeState runtime;
    PersistedSensorSelectionState persisted;
};

// #21, 6.5/6.8: gemeinsame Erstbefuellung fuer Programm-, manuelle und
// Kuehl-Ersatzlauf-Starts. `substitutedFromProduct` ist nur fuer Zeile 2 der
// Startmatrix true (Programmstart); manuelle Starts uebergeben immer false,
// da 6.8 keinen automatischen Ersatz bei Start kennt. Ein produktgefuehrter
// manueller Start mit zum Startzeitpunkt ungueltigem Produkt startet - anders
// als ein abgelehnter Programmstart derselben Konstellation - direkt in
// UserDecisionRequired (6.8: fest wie ProductSensorFailurePolicy::WaitForUser,
// kein Wartetimer). Korrekturauftrag Befund 2: `airSensorValid`/
// `coolingSensorValid` sind jetzt echte Parameter statt einer stillschweigend
// angenommenen Konvention - decideProgramStart/decideManualStart haben dies
// bereits als Vorbedingung erzwungen (hier also immer true), der Kuehl-
// Ersatzlauf (decideStop/decideCompletion) uebergibt jetzt echte Evidenz und
// bleibt ohne sie fail-closed Blocked.
StartSensorSelectionOutcome startSensorSelectionOutcome(
    RunSensorMode effectiveMode, bool substitutedFromProduct,
    bool productSensorValid, bool airSensorValid, bool coolingSensorValid,
    std::uint32_t startRunRevision) {
    StartSensorSelectionOutcome outcome;
    const bool fixedSensorsValid = airSensorValid && coolingSensorValid;
    if (effectiveMode == RunSensorMode::Air) {
        outcome.runtime.permission = fixedSensorsValid
                                         ? SensorPeltierPermission::Allowed
                                         : SensorPeltierPermission::Blocked;
        if (substitutedFromProduct) {
            outcome.runtime.phase = SensorSelectionPhase::AirFallbackActive;
            outcome.persisted.provenance =
                SensorSelectionProvenance::FallbackActive;
        } else {
            outcome.runtime.phase = SensorSelectionPhase::NormalAir;
            outcome.persisted.provenance =
                SensorSelectionProvenance::InitialSelection;
        }
    } else {
        outcome.persisted.provenance =
            SensorSelectionProvenance::InitialSelection;
        if (productSensorValid) {
            outcome.runtime.phase = SensorSelectionPhase::NormalProduct;
            outcome.runtime.permission = fixedSensorsValid
                                             ? SensorPeltierPermission::Allowed
                                             : SensorPeltierPermission::Blocked;
        } else {
            outcome.runtime.phase = SensorSelectionPhase::UserDecisionRequired;
            outcome.runtime.permission = SensorPeltierPermission::Blocked;
        }
    }
    outcome.persisted.lastDecisionCause =
        SensorSelectionDecisionCause::StartSelection;
    outcome.persisted.lastDecisionRunRevision = startRunRevision;
    return outcome;
}

}  // namespace

// #21, 6.14.3: schmale Projektion auf SensorSelectionStateView - kein
// vollstaendiger RunCommandState wird an sensor_selection.hpp weitergereicht.
// External linkage: this is also the sole real projection C5 uses to hand
// the applied #21 selection state to SafetyFaultService.
SensorSelectionStateView sensorSelectionViewFrom(const RunCommandState& state) {
    SensorSelectionStateView view;
    view.activeRunId = state.activeRunId;
    view.runtime = state.sensorSelectionRuntime;
    view.activeMode = state.activeRunSensorMode;
    view.persisted = state.sensorSelection;
    view.runRevision = state.runRevision;
    return view;
}

// #21, 6.14.6: einzige Implementierung; ersetzt das vormalige
// run_commands.cpp::clearActiveRun (nur intern sichtbar) und
// run_persistence_coordinator.cpp::clearCandidateRun (zweite, getrennte
// Implementierung). Der Default von SensorSelectionRuntimeState ist bereits
// der einzige NoActiveRun-/Blocked-Inaktivzustand (sensor_selection_types.hpp),
// daher genuegt die Zuweisung eines frischen Default-Werts.
void clearActiveRunState(RunCommandState& state) {
    state.activeProgramRun.reset();
    state.activeManualRun.reset();
    state.processRunSnapshot.reset();
    state.activeRunId.clear();
    state.activeRunSensorMode.reset();
    state.sensorSelection.reset();
    state.sensorSelectionRuntime = SensorSelectionRuntimeState{};
    // Schema 3 (#18, 5.11/5.14 Punkt 6): dieselben sechs Recovery-/
    // Progressfelder, die ein NoActiveRun-Snapshot zwingend nullopt traegt.
    // recoveryTemperatureEvidence bleibt bewusst unberuehrt (5.20: laufend
    // fortgeschrieben, kein laufgebundenes Diagnosefeld);
    // recoveryEpisodeRevision bleibt aus demselben Grund wie runRevision
    // unberuehrt (monotoner Zaehler ueber Laufgrenzen hinweg).
    state.pendingRecoveryAnchor.reset();
    state.recoveryBootAnchorMonotonicMillis.reset();
    state.lastRecoveryEpisodeEvidence.reset();
    state.priorBootPhaseElapsed.reset();
    state.nominalRecoveryAdjustment.reset();
    state.runProgress = RunProgressState{};
}

// Korrekturauftrag Befund 1: siehe Kommentar in run_commands.hpp. Beide
// Aufrufer (decideApplySensorSelectionAction unten, RunPersistenceCoordinator::
// persistSensorSelection) rufen ausschliesslich diese Funktion fuer die
// mechanische Anwendung einer bereits von applySensorSelectionDecision
// getroffenen Entscheidung auf.
void applySensorSelectionMutation(
    RunCommandState& state, const SensorSelectionStateMutation& mutation) {
    state.sensorSelectionRuntime = mutation.runtime;
    state.activeRunSensorMode = mutation.activeMode;
    state.sensorSelection = mutation.persisted;
    state.runRevision = mutation.resultingRunRevision;
    if (state.activeManualRun.has_value() && mutation.activeMode.has_value()) {
        state.activeManualRun->values.sensorMode = *mutation.activeMode;
    }
}

bool applySensorRoleChangeQualificationReset(RunCommandState& state,
                                             std::uint64_t monotonicMillis) {
    if (state.processState.state != ProcessState::QualifyingTarget) {
        return true;
    }
    if (!state.processRunSnapshot.has_value()) {
        return false;
    }

    const auto reset = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{}, monotonicMillis);
    if (!reset.proposed() ||
        reset.reason != TransitionReason::QualificationReset) {
        return false;
    }
    return applyProcessTransition(state.processState, reset,
                                  &*state.processRunSnapshot);
}

bool validateManualRunPlan(const ManualRunPlan& plan) {
    const auto& values = plan.values;
    const bool waitMatchesPreheat =
        values.preheatEnabled == values.maximumProductWaitMinutes.has_value();
    const bool waitValid = !values.maximumProductWaitMinutes.has_value() ||
                           (*values.maximumProductWaitMinutes >=
                                program_limits::kMinimumProductWaitMinutes &&
                            *values.maximumProductWaitMinutes <=
                                program_limits::kMaximumProductWaitMinutes);
    return run_limits::validRunId(values.runId) &&
           validCommandSource(plan.source) &&
           validSensorMode(values.sensorMode) &&
           plan.kind == ProcessKind::ManualHolding &&
           std::isfinite(values.targetTemperatureCelsius) &&
           values.targetTemperatureCelsius >=
               program_limits::kMinimumFermentationTemperatureCelsius &&
           values.targetTemperatureCelsius <=
               program_limits::kMaximumFermentationTemperatureCelsius &&
           std::isfinite(values.qualificationBandCelsius) &&
           values.qualificationBandCelsius >=
               program_limits::kMinimumQualificationBandCelsius &&
           values.qualificationBandCelsius <=
               program_limits::kMaximumQualificationBandCelsius &&
           values.qualificationDurationMinutes >=
               program_limits::kMinimumQualificationDurationMinutes &&
           values.qualificationDurationMinutes <=
               program_limits::kMaximumQualificationDurationMinutes &&
           values.maximumTargetReachMinutes >=
               program_limits::kMinimumTargetReachMinutes &&
           values.maximumTargetReachMinutes <=
               program_limits::kMaximumTargetReachMinutes &&
           waitMatchesPreheat && waitValid;
}

std::optional<ProcessRunSnapshot> makeProcessRunSnapshot(
    const ManualRunPlan& plan) {
    if (!validateManualRunPlan(plan)) {
        return std::nullopt;
    }
    ProcessRunSnapshot snapshot;
    snapshot.kind = ProcessKind::ManualHolding;
    snapshot.preheatEnabled = plan.values.preheatEnabled;
    snapshot.completionMode = CompletionMode::FinishWithoutCooling;
    snapshot.qualificationDurationMinutes =
        plan.values.qualificationDurationMinutes;
    snapshot.maximumTargetReachMinutes = plan.values.maximumTargetReachMinutes;
    snapshot.maximumProductWaitMinutes = plan.values.maximumProductWaitMinutes;
    return validateProcessRunSnapshot(snapshot)
               ? std::optional<ProcessRunSnapshot>{snapshot}
               : std::nullopt;
}

CommandDecision decideProgramStart(const RunCommandState& current,
                                   const ProgramStartRequest& request) {
    auto decision =
        beginDecision(current, request.envelope, CommandKind::StartProgram);
    if (!requireRunRevision(decision)) {
        return decision;
    }
    if (!request.safetyAllowsStart) {
        decision.status = CommandStatus::SafetyRejected;
        return decision;
    }
    if (current.processState.state != ProcessState::Standby ||
        current.activeProgramRun.has_value() ||
        current.activeManualRun.has_value()) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }
    if (!run_limits::validRunId(request.runId) ||
        !validSensorMode(request.sensorMode)) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    // #21, 6.5: Vorbedingung fuer jede Zeile der Startmatrix - gilt
    // unabhaengig von SensorPreference und angefordertem Modus, kein
    // Sonderfall pro Zeile.
    if (!request.airSensorValid || !request.coolingSensorValid) {
        decision.status = CommandStatus::SafetyRejected;
        return decision;
    }
    const auto resolution = resolveProgramStartSensorMode(
        request.program.program.sensorPreference, request.sensorMode,
        request.productSensorValid);
    if (!resolution.valid) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    auto run = ActiveRun::start(request.program, request.sourceKind,
                                request.sourceProgramRevision);
    if (!run.has_value()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    const auto snapshot = makeProcessRunSnapshot(*run);
    if (!snapshot.has_value()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    // Die Startzusammenfassung entsteht aus vollstaendig validierten Daten und
    // steht deshalb auch fuer eine noch unbestaetigte, aber gueltige Anfrage
    // zur Verfuegung (siehe docs/RUN_COMMANDS.md, "Zusammenfassung vor
    // Bestaetigung"). `before`/`after` bleiben bis hierher identisch mit
    // `current`. `sensorMode` ist ab hier der effektive, bereits gegen die
    // Programmpraeferenz validierte Modus (6.5), nicht mehr der angeforderte.
    decision.startSummary = StartSummary{
        request.runId,
        request.program.program.name,
        run->effectiveValues().targetTemperatureCelsius,
        run->effectiveValues().remainingDurationMinutes,
        resolution.effectiveMode,
        request.program.program.preheat,
        request.program.program.completion.mode,
        ProcessKind::Timed,
    };
    if (resolution.substituted) {
        // #21, 6.5 Zeile 2/6.11: Vorschau vor Bestaetigung - die Laufrevision
        // ist die ohnehin beim Start erzeugte erste Revision, hier noch
        // prospektiv (keine zweite Revision, kein zusaetzlicher Mutations-
        // schritt). Bei bereits erschoepfter Kapazitaet bleibt der Wert
        // unveraendert; requireRevisionCapacity lehnt die Anfrage unten
        // ohnehin ab, bevor irgendetwas commitfaehig wird.
        const auto max = std::numeric_limits<std::uint32_t>::max();
        const auto previewRevision = decision.before.runRevision == max
                                         ? max
                                         : decision.before.runRevision + 1U;
        decision.startSensorSelectionNotice = StartSensorSelectionNotice{
            request.sensorMode, resolution.effectiveMode, previewRevision};
    }

    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
        return decision;
    }

    if (!requireRevisionCapacity(decision, decision.before.runRevision)) {
        return decision;
    }

    if (!applyTransition(decision.after, ProcessEvent::StartRun,
                         request.envelope.monotonicMillis, &*snapshot)) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    decision.after.activeRunId = request.runId;
    decision.after.activeRunSensorMode = resolution.effectiveMode;
    decision.after.activeProgramRun = std::move(run);
    decision.after.activeManualRun.reset();
    decision.after.processRunSnapshot = *snapshot;
    beginMutation(decision);
    ++decision.after.runRevision;
    const auto outcome = startSensorSelectionOutcome(
        resolution.effectiveMode, resolution.substituted,
        request.productSensorValid, request.airSensorValid,
        request.coolingSensorValid, decision.after.runRevision);
    decision.after.sensorSelectionRuntime = outcome.runtime;
    decision.after.sensorSelection = outcome.persisted;
    static_cast<void>(addEffect(decision, CommandEffect::RunStarted));
    return decision;
}

CommandDecision decideManualStart(const RunCommandState& current,
                                  const ManualStartRequest& request) {
    auto decision = beginDecision(current, request.envelope,
                                  CommandKind::StartManualHolding);
    if (!requireRunRevision(decision)) {
        return decision;
    }
    if (!request.safetyAllowsStart) {
        decision.status = CommandStatus::SafetyRejected;
        return decision;
    }
    if (current.processState.state != ProcessState::Standby ||
        current.activeProgramRun.has_value() ||
        current.activeManualRun.has_value()) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }
    auto plan = makeManualPlan(request.plan, request.envelope);
    if (!plan.has_value()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    // #21, 6.5/6.8: dieselbe Vorbedingung wie fuer Programmstarts - gilt fuer
    // jeden Start, nicht nur programmgefuehrte.
    if (!request.airSensorValid || !request.coolingSensorValid) {
        decision.status = CommandStatus::SafetyRejected;
        return decision;
    }

    // Wie bei decideProgramStart: die Zusammenfassung entsteht aus dem
    // vollstaendig validierten Plan und steht deshalb auch fuer eine noch
    // unbestaetigte, aber gueltige Anfrage zur Verfuegung. `installManualRun`
    // wird erst nach der Bestaetigungspruefung aufgerufen, damit `before`/
    // `after` bis dahin identisch mit `current` bleiben.
    decision.startSummary = StartSummary{
        request.plan.runId,
        request.plan.runId,
        request.plan.targetTemperatureCelsius,
        std::nullopt,
        request.plan.sensorMode,
        request.plan.preheatEnabled,
        CompletionMode::FinishWithoutCooling,
        ProcessKind::ManualHolding,
    };

    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
        return decision;
    }

    if (!requireRevisionCapacity(decision, decision.before.runRevision)) {
        return decision;
    }

    if (!installManualRun(decision.after, decision.envelope,
                          std::move(*plan))) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    beginMutation(decision);
    ++decision.after.runRevision;
    // #21, 6.8: kein automatischer Ersatz bei manuellem Start - substituted
    // ist immer false. Ein produktgefuehrter Start mit ungueltigem Produkt
    // startet direkt in UserDecisionRequired (siehe
    // startSensorSelectionOutcome), kein Wartetimer, keine
    // StartSensorSelectionNotice.
    const auto outcome = startSensorSelectionOutcome(
        request.plan.sensorMode, false, request.productSensorValid,
        request.airSensorValid, request.coolingSensorValid,
        decision.after.runRevision);
    decision.after.sensorSelectionRuntime = outcome.runtime;
    decision.after.sensorSelection = outcome.persisted;
    static_cast<void>(addEffect(decision, CommandEffect::ManualRunStarted));
    static_cast<void>(addEffect(decision, CommandEffect::RunStarted));
    return decision;
}

bool validStopOption(StopOption option) {
    switch (option) {
        case StopOption::Back:
        case StopOption::AbortAndTurnOff:
        case StopOption::AbortAndCool:
            return true;
    }
    return false;
}

CommandDecision decideStop(const RunCommandState& current,
                           const StopRequest& request) {
    auto decision =
        beginDecision(current, request.envelope, CommandKind::AbortAndTurnOff);
    if (!decision.proposed()) {
        return decision;
    }
    if (!validStopOption(request.option)) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    if (request.option == StopOption::Back) {
        decision.status = CommandStatus::NoChange;
        return decision;
    }
    if (request.option == StopOption::AbortAndCool) {
        decision.kind = CommandKind::AbortAndCool;
    }
    if (!requireRunRevision(decision)) {
        return decision;
    }
    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
        return decision;
    }
    if (!activeProcessState(current.processState.state) ||
        !current.processRunSnapshot.has_value()) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }

    std::optional<ManualRunPlan> coolingPlan;
    if (request.option == StopOption::AbortAndCool) {
        if (!request.safetyAllowsCooling) {
            decision.status = CommandStatus::SafetyRejected;
            return decision;
        }
        if (!request.coolingPlan.has_value()) {
            decision.status = CommandStatus::ContextMissing;
            return decision;
        }
        coolingPlan = makeManualPlan(*request.coolingPlan, request.envelope);
        if (!coolingPlan.has_value()) {
            decision.status = CommandStatus::InvalidInput;
            return decision;
        }
    }

    if (!requireRevisionCapacity(decision, decision.before.runRevision)) {
        return decision;
    }

    auto candidate = decision.before;
    if (!applyTransition(candidate, ProcessEvent::Abort,
                         request.envelope.monotonicMillis,
                         &*current.processRunSnapshot)) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }
    clearActiveRunState(candidate);

    if (coolingPlan.has_value() &&
        !installManualRun(candidate, decision.envelope,
                          std::move(*coolingPlan))) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    decision.after = std::move(candidate);
    beginMutation(decision);
    ++decision.after.runRevision;
    // #21, 6.5/6.14.6: der Kuehl-Ersatzlauf ist ebenfalls ein aktiver Lauf
    // und braucht deshalb dieselbe Erstbefuellung wie jeder andere Start
    // (clearActiveRunState hat sie oben geleert). Kein neues externes
    // Produktsensorsignal fuer diesen Pfad (kein productSensorValid);
    // Kuehlplaene sind konventionell Luft-gefuehrt, und
    // startSensorSelectionOutcome faellt fuer einen (strukturell weiterhin
    // moeglichen) Produktmodus fail-closed auf UserDecisionRequired/Blocked
    // zurueck. Korrekturauftrag Befund 2: Air-/Cooling-Permission wird nicht
    // mehr per Konvention Allowed gesetzt, sondern verlangt jetzt dieselbe
    // explizite Evidenz wie jeder andere Start (request.airSensorValid/
    // request.coolingSensorValid) - ohne sie bleibt der Ersatzlauf
    // fail-closed Blocked.
    if (decision.after.activeManualRun.has_value()) {
        const auto outcome = startSensorSelectionOutcome(
            decision.after.activeManualRun->values.sensorMode, false, false,
            request.airSensorValid, request.coolingSensorValid,
            decision.after.runRevision);
        decision.after.sensorSelectionRuntime = outcome.runtime;
        decision.after.sensorSelection = outcome.persisted;
    }
    static_cast<void>(addEffect(decision, CommandEffect::RunAborted));
    static_cast<void>(
        addEffect(decision, CommandEffect::SafePeltierStopRequested));
    static_cast<void>(addEffect(decision, CommandEffect::FanRunOnRequired));
    if (request.option == StopOption::AbortAndCool) {
        static_cast<void>(addEffect(decision, CommandEffect::ManualRunStarted));
        static_cast<void>(addEffect(decision, CommandEffect::RunStarted));
    }
    return decision;
}

CommandDecision decideCompletion(const RunCommandState& current,
                                 const CompletionRequest& request) {
    const auto kind = request.startCooling ? CommandKind::CoolAfterCompletion
                                           : CommandKind::AcknowledgeCompletion;
    auto decision = beginDecision(current, request.envelope, kind);
    if (!requireRunRevision(decision)) {
        return decision;
    }
    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
        return decision;
    }
    if (current.processState.state != ProcessState::Completed) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }

    std::optional<ManualRunPlan> coolingPlan;
    if (request.startCooling) {
        if (!request.safetyAllowsCooling) {
            decision.status = CommandStatus::SafetyRejected;
            return decision;
        }
        if (!request.coolingPlan.has_value()) {
            decision.status = CommandStatus::ContextMissing;
            return decision;
        }
        coolingPlan = makeManualPlan(*request.coolingPlan, request.envelope);
        if (!coolingPlan.has_value()) {
            decision.status = CommandStatus::InvalidInput;
            return decision;
        }
    }

    if (!requireRevisionCapacity(decision, decision.before.runRevision)) {
        return decision;
    }

    auto candidate = decision.before;
    if (!applyTransition(candidate, ProcessEvent::AcknowledgeCompletion,
                         request.envelope.monotonicMillis, nullptr)) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }
    clearActiveRunState(candidate);
    if (coolingPlan.has_value() &&
        !installManualRun(candidate, decision.envelope,
                          std::move(*coolingPlan))) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    decision.after = std::move(candidate);
    beginMutation(decision);
    ++decision.after.runRevision;
    // #21, 6.5/6.14.6: siehe decideStop - derselbe Kuehl-Ersatzlauf-Fall
    // nach regulaerem Abschluss.
    if (decision.after.activeManualRun.has_value()) {
        const auto outcome = startSensorSelectionOutcome(
            decision.after.activeManualRun->values.sensorMode, false, false,
            request.airSensorValid, request.coolingSensorValid,
            decision.after.runRevision);
        decision.after.sensorSelectionRuntime = outcome.runtime;
        decision.after.sensorSelection = outcome.persisted;
    }
    static_cast<void>(
        addEffect(decision, CommandEffect::CompletionAcknowledged));
    if (request.startCooling) {
        static_cast<void>(addEffect(decision, CommandEffect::ManualRunStarted));
        static_cast<void>(addEffect(decision, CommandEffect::RunStarted));
    }
    return decision;
}

CommandDecision decideRunAdjustment(
    const RunCommandState& current,
    const RunAdjustmentCommandRequest& request) {
    auto decision =
        beginDecision(current, request.envelope, CommandKind::AdjustRun);
    if (!requireRunRevision(decision)) {
        return decision;
    }
    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
        return decision;
    }
    if (!adjustmentAllowedIn(current.processState.state) ||
        !current.activeProgramRun.has_value() ||
        !current.processRunSnapshot.has_value()) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }

    RunAdjustmentRequest adjustment;
    adjustment.targetTemperatureCelsius = request.targetTemperatureCelsius;
    adjustment.remainingDurationMinutes = request.remainingDurationMinutes;
    adjustment.confirmed = true;
    adjustment.source = changeSource(request.envelope.source);
    adjustment.reason = RunChangeReason::UserAdjustment;
    adjustment.timestamp.monotonicMillis = request.envelope.monotonicMillis;
    RunAdjustmentContext context;
    context.runActive = true;
    context.safetyAllowsChange = request.safetyAllowsChange;
    context.phaseContext = phaseContextFor(current.processState.state);
    const auto runDecision =
        current.activeProgramRun->decideAdjustment(adjustment, context);
    if (!runDecision.proposed()) {
        decision.status = mapAdjustmentStatus(runDecision.status);
        return decision;
    }

    if (!requireRevisionCapacity(decision, decision.before.runRevision)) {
        return decision;
    }

    const auto beforeValues = current.activeProgramRun->effectiveValues();
    const bool durationChanged = runDecision.revision.has_value() &&
                                 runDecision.revision->remainingDurationChanged;
    std::optional<std::uint32_t> observedDelta;
    if (durationChanged &&
        current.processState.state == ProcessState::Fermenting) {
        observedDelta = deriveFermentingSecondsDelta(
            current, request.envelope.monotonicMillis);
        if (!observedDelta.has_value()) {
            decision.status = CommandStatus::InvalidInput;
            return decision;
        }
    }
    auto candidate = decision.before;
    if (durationChanged &&
        current.processState.state == ProcessState::Fermenting) {
        if (!observedDelta.has_value() ||
            !foldObservedRunSeconds(candidate, *observedDelta)) {
            decision.status = CommandStatus::InvalidInput;
            return decision;
        }
    }
    if (!candidate.activeProgramRun.has_value()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    auto& adjustedRun = candidate.activeProgramRun.value();
    const auto applied = adjustedRun.applyAdjustment(runDecision);
    if (!applied.applied()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    const auto afterValues = adjustedRun.effectiveValues();
    const bool targetChanged = beforeValues.targetTemperatureCelsius !=
                               afterValues.targetTemperatureCelsius;
    candidate.processRunSnapshot = makeProcessRunSnapshot(adjustedRun);
    if (!candidate.processRunSnapshot.has_value()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    if (targetChanged &&
        current.processState.state != ProcessState::Fermenting &&
        !applyTransition(candidate, ProcessEvent::TargetChanged,
                         request.envelope.monotonicMillis,
                         &*candidate.processRunSnapshot)) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    decision.committedControlContextTransition =
        targetContextTransition(targetChanged);
    if (durationChanged &&
        current.processState.state == ProcessState::Fermenting) {
        candidate.processState.stateEnteredAtMillis =
            request.envelope.monotonicMillis;
        candidate.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
            ProcessState::Fermenting, PriorBootPhaseElapsed{0U, 0U}};
        candidate.nominalRecoveryAdjustment.reset();
        candidate.pendingRecoveryAnchor.reset();
        candidate.recoveryBootAnchorMonotonicMillis.reset();
        if (candidate.lastRecoveryEpisodeEvidence.has_value()) {
            supersedeUnbookedWeightedSegment(
                candidate.runProgress, candidate.lastRecoveryEpisodeEvidence
                                           ->weightedProgressSegmentId);
            candidate.lastRecoveryEpisodeEvidence->weightedProgressSegmentId
                .reset();
        }
    }

    decision.after = std::move(candidate);
    beginMutation(decision);
    ++decision.after.runRevision;
    decision.adjustmentPreview = RunAdjustmentPreview{
        beforeValues,
        afterValues,
        current.processState.state,
        applied.effect == RunAdjustmentEffect::RestartTargetQualification,
        applied.effect ==
            RunAdjustmentEffect::ContinueFermentationWithoutRequalification,
    };
    static_cast<void>(addEffect(decision, CommandEffect::RunAdjusted));
    return decision;
}

CommandDecision decideApplyRecoveryTimeCorrection(
    const RunCommandState& current,
    const ApplyRecoveryTimeCorrectionRequest& request) {
    auto decision = beginDecision(current, request.envelope,
                                  CommandKind::ApplyRecoveryTimeCorrection);
    if (!requireRunRevision(decision) ||
        !requireRecoveryEpisodeRevision(decision)) {
        return decision;
    }
    if (!request.envelope.confirmed ||
        current.processState.state != ProcessState::Fermenting ||
        !current.priorBootPhaseElapsed.has_value() ||
        current.priorBootPhaseElapsed->taggedState !=
            ProcessState::Fermenting ||
        !current.priorBootPhaseElapsed->elapsed.upperBoundSeconds.has_value()) {
        decision.status = request.envelope.confirmed
                              ? CommandStatus::NotAllowedInState
                              : CommandStatus::NotConfirmed;
        return decision;
    }

    const auto currentAdjustment = current.nominalRecoveryAdjustment.has_value()
                                       ? *current.nominalRecoveryAdjustment
                                       : NominalRecoveryAdjustmentState{};
    if (current.nominalRecoveryAdjustment.has_value() &&
        currentAdjustment.lastAppliedEpisodeRevision ==
            current.recoveryEpisodeRevision) {
        decision.status =
            currentAdjustment.lastAppliedEpisodeDelta == request.secondsDelta
                ? CommandStatus::AlreadyProcessed
                : CommandStatus::NotAllowedInState;
        return decision;
    }

    if (request.secondsDelta > std::numeric_limits<std::uint32_t>::max() -
                                   currentAdjustment.cumulativeAppliedSeconds) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    const auto newCumulative =
        currentAdjustment.cumulativeAppliedSeconds + request.secondsDelta;
    const auto lower =
        static_cast<std::uint64_t>(
            current.priorBootPhaseElapsed->elapsed.lowerBoundSeconds) +
        newCumulative;
    if (lower > *current.priorBootPhaseElapsed->elapsed.upperBoundSeconds) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    beginMutation(decision);
    decision.after.nominalRecoveryAdjustment = NominalRecoveryAdjustmentState{
        newCumulative, current.recoveryEpisodeRevision, request.secondsDelta};
    return decision;
}

CommandDecision decideAcknowledgeMessage(const RunCommandState& current,
                                         const MessageCommandRequest& request) {
    auto decision = beginDecision(current, request.envelope,
                                  CommandKind::AcknowledgeMessage);
    if (!requireMessageRevision(decision)) {
        return decision;
    }
    auto* message = findMessage(decision.after, request.messageId);
    if (message == nullptr) {
        decision.status = CommandStatus::ContextMissing;
        return decision;
    }
    if (message->acknowledged) {
        decision.status = CommandStatus::NoChange;
        return decision;
    }
    if (!requireRevisionCapacity(decision, message->revision) ||
        !requireRevisionCapacity(decision, decision.after.messageRevision)) {
        return decision;
    }
    beginMutation(decision);
    message->acknowledged = true;
    ++message->revision;
    ++decision.after.messageRevision;
    static_cast<void>(addEffect(decision, CommandEffect::MessageAcknowledged));
    return decision;
}

CommandDecision decideMuteMessage(const RunCommandState& current,
                                  const MessageCommandRequest& request) {
    auto decision =
        beginDecision(current, request.envelope, CommandKind::MuteMessage);
    if (!requireMessageRevision(decision)) {
        return decision;
    }
    auto* message = findMessage(decision.after, request.messageId);
    if (message == nullptr) {
        decision.status = CommandStatus::ContextMissing;
        return decision;
    }
    if (message->acousticMuted) {
        decision.status = CommandStatus::NoChange;
        return decision;
    }
    if (!requireRevisionCapacity(decision, message->revision) ||
        !requireRevisionCapacity(decision, decision.after.messageRevision)) {
        return decision;
    }
    beginMutation(decision);
    message->acousticMuted = true;
    ++message->revision;
    ++decision.after.messageRevision;
    static_cast<void>(addEffect(decision, CommandEffect::AcousticMuted));
    return decision;
}

namespace {

CommandDecision decideFaultResetInternal(const RunCommandState& current,
                                         const FaultResetRequest& request) {
    auto decision =
        beginDecision(current, request.envelope, CommandKind::ResetFault);
    if (!requireFaultRevision(decision)) {
        return decision;
    }
    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
        return decision;
    }
    if (!request.targetFault.valid() ||
        !request.envelope.expectedFaultRevision.has_value()) {
        decision.status = CommandStatus::SafetyRejected;
        return decision;
    }

    const auto& target = request.targetFault;
    const FaultRecord* targetRecord = nullptr;
    for (std::size_t index = 0U; index < current.faultSnapshot.count; ++index) {
        if (current.faultSnapshot.records[index].instanceId == target) {
            targetRecord = &current.faultSnapshot.records[index];
            break;
        }
    }
    if (targetRecord == nullptr ||
        targetRecord->status == FaultStatus::Cleared) {
        decision.status = CommandStatus::ContextMissing;
        return decision;
    }
    // D1: compare against the target fault's own expected revision, not the
    // shared FaultCore revision counter. Another fault advancing the global
    // counter without touching this target must not make a valid reset of
    // this target look stale.
    if (targetRecord->faultRevision !=
        *request.envelope.expectedFaultRevision) {
        decision.status = CommandStatus::StaleState;
        return decision;
    }
    if (targetRecord->causeActive || !targetRecord->latched ||
        targetRecord->status != FaultStatus::CauseClearedLocked) {
        decision.status = CommandStatus::SafetyRejected;
        return decision;
    }
    for (std::size_t index = 0U; index < current.faultSnapshot.count; ++index) {
        const auto& other = current.faultSnapshot.records[index];
        if (other.instanceId != target && isBlockingFault(other)) {
            decision.status = CommandStatus::SafetyRejected;
            return decision;
        }
    }
    if (!requireRevisionCapacity(decision, decision.before.faultRevision)) {
        return decision;
    }
    beginMutation(decision);
    ++decision.after.faultRevision;
    for (std::size_t index = 0U; index < decision.after.faultSnapshot.count;
         ++index) {
        auto& record = decision.after.faultSnapshot.records[index];
        if (record.instanceId == request.targetFault) {
            record.status = FaultStatus::Cleared;
            record.causeActive = false;
            record.faultRevision = decision.after.faultRevision;
            break;
        }
    }
    decision.after.faultSnapshot.revision = decision.after.faultRevision;
    decision.after.criticalSafetyEventPending = false;
    for (std::size_t index = 0U; index < decision.after.faultSnapshot.count;
         ++index) {
        if (isBlockingFault(decision.after.faultSnapshot.records[index])) {
            decision.after.criticalSafetyEventPending = true;
            break;
        }
    }
    decision.after.faultSnapshot.criticalSafetyEventPending =
        decision.after.criticalSafetyEventPending;
    static_cast<void>(addEffect(decision, CommandEffect::FaultResetAuthorized));
    return decision;
}

}  // namespace

CommandDecision decideFaultReset(const RunCommandState& current,
                                 const FaultResetRequest& request) {
    return decideFaultResetInternal(current, request);
}

void applyFaultCoreProjection(RunCommandState& state,
                              const FaultCore& faultCore) {
    state.faultSnapshot = faultCore.snapshot();
    state.faultRevision = state.faultSnapshot.revision;
    state.criticalSafetyEventPending =
        state.faultSnapshot.criticalSafetyEventPending;
}

CommandDecision decideApplySensorSelectionAction(
    const RunCommandState& current,
    const SensorSelectionCommandRequest& request,
    const CrossRolePlausibilityContext& plausibility) {
    auto decision = beginDecision(current, request.envelope,
                                  CommandKind::ApplySensorSelectionAction);
    if (!requireRunRevision(decision)) {
        return decision;
    }
    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
        return decision;
    }
    if (!current.activeProgramRun.has_value() &&
        !current.activeManualRun.has_value()) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }
    if (current.activeRunId.empty() ||
        !current.activeRunSensorMode.has_value()) {
        decision.status = CommandStatus::ContextMissing;
        return decision;
    }
    // 6.14.1: `safetyAllowsChange` ist ein zusaetzliches externes
    // Pruefsignal (analog `safetyAllowsStart`/`safetyAllowsCooling`) und wird
    // unabhaengig von der folgenden internen Matrix verlangt.
    if (!request.safetyAllowsChange) {
        decision.status = CommandStatus::SafetyRejected;
        decision.sensorSelectionApplyStatus =
            SensorSelectionApplyStatus::InvalidDecision;
        return decision;
    }
    // 6.14.1 Aktionsspezifische Matrix bei criticalSafetyEventPending:
    // ContinueWithAir/ReturnToProduct enden hier fail-closed, bevor der
    // Automat ueberhaupt aufgerufen wird - kein Ausweg aus einer offenen
    // Sicherheitslage ueber eine Komfortaktion. RecheckProduct darf die reine
    // Pruefung ausfuehren (unten); eine daraus entstehende Mutation wird dort
    // verworfen.
    if (current.criticalSafetyEventPending &&
        (request.action == SensorSelectionUserAction::ContinueWithAir ||
         request.action == SensorSelectionUserAction::ReturnToProduct)) {
        decision.status = CommandStatus::SafetyRejected;
        decision.sensorSelectionApplyStatus =
            SensorSelectionApplyStatus::InvalidDecision;
        return decision;
    }

    const auto view = sensorSelectionViewFrom(current);
    SensorSelectionDecision selectionDecision;
    selectionDecision.expected = view;
    selectionDecision.program = sensorSelectionProgramContextFor(current);
    selectionDecision.plausibility = plausibility;
    selectionDecision.userAction = request.action;

    const auto mutation = applySensorSelectionDecision(
        view, selectionDecision, request.envelope.monotonicMillis);
    decision.sensorSelectionApplyStatus = mutation.status;

    if (current.criticalSafetyEventPending &&
        request.action == SensorSelectionUserAction::RecheckProduct &&
        mutation.status != SensorSelectionApplyStatus::NoChange) {
        // Pruefung durfte laufen (6.14.1); jede daraus entstehende Modus-
        // oder Permissionmutation bleibt bei offenem kritischem Safety-
        // Ereignis verworfen - kein Write, kein RAM-Apply.
        decision.status = CommandStatus::SafetyRejected;
        return decision;
    }

    switch (mutation.status) {
        case SensorSelectionApplyStatus::NoChange:
            decision.status = CommandStatus::NoChange;
            return decision;
        case SensorSelectionApplyStatus::AppliedPersistentCandidate:
        case SensorSelectionApplyStatus::AppliedRamOnly: {
            auto candidate = decision.before;
            // Korrekturauftrag Befund 1: gemeinsamer mechanischer
            // Mutationshelfer mit dem automatischen Pfad (siehe
            // run_persistence_coordinator.cpp::persistSensorSelection).
            applySensorSelectionMutation(candidate, mutation);
            if (mutation.event.has_value() &&
                resolveControlSensorRoleTransition(
                    decision.before.processState.state,
                    decision.before.activeRunSensorMode,
                    candidate.processState.state, candidate.activeRunSensorMode)
                    .has_value() &&
                !applySensorRoleChangeQualificationReset(
                    candidate, request.envelope.monotonicMillis)) {
                decision.status = CommandStatus::InvalidInput;
                return decision;
            }
            decision.after = std::move(candidate);
            beginMutation(decision);
            if (mutation.event.has_value()) {
                decision.sensorSelectionEvent = mutation.event;
                decision.committedControlContextTransition =
                    resolveControlSensorRoleTransition(
                        decision.before.processState.state,
                        decision.before.activeRunSensorMode,
                        decision.after.processState.state,
                        decision.after.activeRunSensorMode);
            } else if (mutation.notice.has_value()) {
                decision.sensorSelectionNotice = mutation.notice;
            }
            // Permission-Uebergang statt Ursachenliste: anders als im
            // automatischen Pfad (persistSensorSelection, genau sechs
            // bekannte Ursachen) kann der manuelle Pfad ueber
            // ManualUserFallback/ManualUserReturn ebenfalls Blocked<->Allowed
            // aendern - eine Ursachenliste wuerde diese Uebergaenge verfehlen.
            // #21, 6.14.4: der Effekt transportiert ausschliesslich, dass
            // peltierPermission sich geaendert hat, keine direkte
            // Aktorfreigabe.
            if (decision.before.sensorSelectionRuntime.permission !=
                mutation.runtime.permission) {
                const auto effect =
                    mutation.runtime.permission ==
                            SensorPeltierPermission::Blocked
                        ? CommandEffect::SensorSelectionPermissionBlocked
                        : CommandEffect::SensorSelectionPermissionRestored;
                static_cast<void>(addEffect(decision, effect));
            }
            return decision;
        }
        case SensorSelectionApplyStatus::StaleDecision:
        case SensorSelectionApplyStatus::InvalidDecision:
        case SensorSelectionApplyStatus::InvalidContext:
        case SensorSelectionApplyStatus::TimeWentBackwards:
        case SensorSelectionApplyStatus::CapacityReached:
            decision.status = mapSensorSelectionRejection(mutation.status);
            return decision;
    }
    decision.status = CommandStatus::InvalidInput;
    return decision;
}

CommandStatus applyRunCommand(RunCommandState& current,
                              const CommandDecision& decision) {
    if (containsProcessedCommand(current, decision.envelope.id)) {
        return CommandStatus::AlreadyProcessed;
    }
    if (current.criticalSafetyEventPending &&
        isRunComfortCommand(decision.kind)) {
        return CommandStatus::SafetyRejected;
    }
    if (!decision.proposed() ||
        current.commandSequence != decision.before.commandSequence ||
        !equalProcessRuntimeState(current.processState,
                                  decision.before.processState) ||
        current.runRevision != decision.before.runRevision ||
        current.messageRevision != decision.before.messageRevision ||
        current.faultRevision != decision.before.faultRevision ||
        !equalFaultCoreSnapshot(current.faultSnapshot,
                                decision.before.faultSnapshot) ||
        current.criticalSafetyEventPending !=
            decision.before.criticalSafetyEventPending ||
        current.activeRunId != decision.before.activeRunId ||
        current.activeRunSensorMode != decision.before.activeRunSensorMode ||
        // #21, 6.14.2 (Review-Blocking 1): ohne diese zwei Felder wuerde ein
        // zwischen Entscheidung und Anwendung durch eine parallele
        // automatische Bewertung veraenderter Auswahlzustand (z. B. ein
        // inzwischen eingetretener SafeLocked-Zustand) von einer noch auf
        // dem alten Zustand basierenden Kommandoentscheidung stillschweigend
        // ueberschrieben - ein Sicherheits-Lock koennte verloren gehen. Gilt
        // fuer jeden CommandKind, nicht nur ApplySensorSelectionAction.
        current.sensorSelectionRuntime !=
            decision.before.sensorSelectionRuntime ||
        current.sensorSelection != decision.before.sensorSelection ||
        current.recoveryEpisodeRevision !=
            decision.before.recoveryEpisodeRevision ||
        decision.after.commandSequence != current.commandSequence + 1U) {
        return CommandStatus::StaleState;
    }

    current = decision.after;
    return CommandStatus::Applied;
}

const RuntimeMessage* highestPriorityActiveMessage(
    const RunCommandState& state) {
    const RuntimeMessage* highest = nullptr;
    for (std::size_t index = 0U; index < state.messageCount; ++index) {
        const auto& candidate = state.messages[index];
        if (!candidate.active || candidate.resolved) {
            continue;
        }
        if (highest == nullptr ||
            static_cast<std::uint8_t>(candidate.messageClass) >
                static_cast<std::uint8_t>(highest->messageClass) ||
            (candidate.messageClass == highest->messageClass &&
             candidate.priority > highest->priority)) {
            highest = &candidate;
        }
    }
    return highest;
}

}  // namespace fermentation
