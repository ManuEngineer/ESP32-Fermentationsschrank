#include "run_commands.hpp"

#include <cmath>
#include <limits>
#include <utility>

#include "program_limits.hpp"

namespace fermentation {
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
            return true;
        case CommandKind::AcknowledgeMessage:
        case CommandKind::MuteMessage:
        case CommandKind::ResetFault:
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
    } else {
        decision.after.commandSequence = current.commandSequence + 1U;
    }
    return decision;
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
    if (*decision.envelope.expectedFaultRevision !=
        decision.before.faultRevision) {
        decision.status = CommandStatus::StaleState;
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

void clearActiveRun(RunCommandState& state) {
    state.activeProgramRun.reset();
    state.activeManualRun.reset();
    state.processRunSnapshot.reset();
    state.activeRunId.clear();
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

bool installManualRun(CommandDecision& decision, ManualRunPlan plan) {
    const auto snapshot = makeProcessRunSnapshot(plan);
    if (!snapshot.has_value() ||
        !applyTransition(decision.after, ProcessEvent::StartRun,
                         decision.envelope.monotonicMillis, &*snapshot)) {
        decision.status = CommandStatus::InvalidInput;
        return false;
    }
    decision.after.activeProgramRun.reset();
    decision.after.activeRunId = plan.values.runId;
    decision.after.activeManualRun = std::move(plan);
    decision.after.processRunSnapshot = *snapshot;
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

}  // namespace

bool validateManualRunPlan(const ManualRunPlan& plan) {
    const auto& values = plan.values;
    const bool waitMatchesPreheat =
        values.preheatEnabled == values.maximumProductWaitMinutes.has_value();
    const bool waitValid = !values.maximumProductWaitMinutes.has_value() ||
                           (*values.maximumProductWaitMinutes >=
                                program_limits::kMinimumProductWaitMinutes &&
                            *values.maximumProductWaitMinutes <=
                                program_limits::kMaximumProductWaitMinutes);
    return !values.runId.empty() && validCommandSource(plan.source) &&
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
    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
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
    if (request.runId.empty() || !validSensorMode(request.sensorMode)) {
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
    if (!snapshot.has_value() ||
        !applyTransition(decision.after, ProcessEvent::StartRun,
                         request.envelope.monotonicMillis, &*snapshot)) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    decision.after.activeRunId = request.runId;
    decision.after.activeProgramRun = std::move(run);
    decision.after.activeManualRun.reset();
    decision.after.processRunSnapshot = *snapshot;
    ++decision.after.runRevision;
    decision.startSummary = StartSummary{
        request.runId,
        request.program.program.name,
        decision.after.activeProgramRun->effectiveValues()
            .targetTemperatureCelsius,
        decision.after.activeProgramRun->effectiveValues()
            .remainingDurationMinutes,
        request.sensorMode,
        request.program.program.preheat,
        request.program.program.completion.mode,
        ProcessKind::Timed,
    };
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
    if (!request.envelope.confirmed) {
        decision.status = CommandStatus::NotConfirmed;
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
    if (!plan.has_value() || !installManualRun(decision, std::move(*plan))) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    ++decision.after.runRevision;
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
    static_cast<void>(addEffect(decision, CommandEffect::ManualRunStarted));
    static_cast<void>(addEffect(decision, CommandEffect::RunStarted));
    return decision;
}

CommandDecision decideStop(const RunCommandState& current,
                           const StopRequest& request) {
    if (request.option == StopOption::Back) {
        return result(current, request.envelope, CommandKind::AbortAndTurnOff,
                      CommandStatus::NoChange);
    }
    const auto kind = request.option == StopOption::AbortAndCool
                          ? CommandKind::AbortAndCool
                          : CommandKind::AbortAndTurnOff;
    auto decision = beginDecision(current, request.envelope, kind);
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

    if (!applyTransition(decision.after, ProcessEvent::Abort,
                         request.envelope.monotonicMillis,
                         &*current.processRunSnapshot)) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }
    clearActiveRun(decision.after);
    static_cast<void>(addEffect(decision, CommandEffect::RunAborted));
    static_cast<void>(
        addEffect(decision, CommandEffect::SafePeltierStopRequested));
    static_cast<void>(addEffect(decision, CommandEffect::FanRunOnRequired));

    if (coolingPlan.has_value() &&
        !installManualRun(decision, std::move(*coolingPlan))) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    if (request.option == StopOption::AbortAndCool) {
        static_cast<void>(addEffect(decision, CommandEffect::ManualRunStarted));
        static_cast<void>(addEffect(decision, CommandEffect::RunStarted));
    }
    ++decision.after.runRevision;
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

    if (!applyTransition(decision.after, ProcessEvent::AcknowledgeCompletion,
                         request.envelope.monotonicMillis, nullptr)) {
        decision.status = CommandStatus::NotAllowedInState;
        return decision;
    }
    clearActiveRun(decision.after);
    static_cast<void>(
        addEffect(decision, CommandEffect::CompletionAcknowledged));
    if (coolingPlan.has_value() &&
        !installManualRun(decision, std::move(*coolingPlan))) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    if (request.startCooling) {
        static_cast<void>(addEffect(decision, CommandEffect::ManualRunStarted));
        static_cast<void>(addEffect(decision, CommandEffect::RunStarted));
    }
    ++decision.after.runRevision;
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
    const auto runDecision =
        current.activeProgramRun->decideAdjustment(adjustment, context);
    if (!runDecision.proposed()) {
        decision.status = mapAdjustmentStatus(runDecision.status);
        return decision;
    }

    const auto beforeValues = current.activeProgramRun->effectiveValues();
    if (!decision.after.activeProgramRun.has_value()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    auto& adjustedRun = decision.after.activeProgramRun.value();
    const auto applied = adjustedRun.applyAdjustment(runDecision);
    if (!applied.applied()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    const auto afterValues = adjustedRun.effectiveValues();
    const bool targetChanged = beforeValues.targetTemperatureCelsius !=
                               afterValues.targetTemperatureCelsius;
    const bool durationChanged = beforeValues.remainingDurationMinutes !=
                                 afterValues.remainingDurationMinutes;
    decision.after.processRunSnapshot = makeProcessRunSnapshot(adjustedRun);
    if (!decision.after.processRunSnapshot.has_value()) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }

    if (targetChanged &&
        current.processState.state != ProcessState::Fermenting &&
        !applyTransition(decision.after, ProcessEvent::TargetChanged,
                         request.envelope.monotonicMillis,
                         &*decision.after.processRunSnapshot)) {
        decision.status = CommandStatus::InvalidInput;
        return decision;
    }
    if (durationChanged &&
        current.processState.state == ProcessState::Fermenting) {
        decision.after.processState.stateEnteredAtMillis =
            request.envelope.monotonicMillis;
    }

    ++decision.after.runRevision;
    decision.adjustmentPreview = RunAdjustmentPreview{
        beforeValues,
        afterValues,
        current.processState.state,
        targetChanged && current.processState.state != ProcessState::Fermenting,
        targetChanged && current.processState.state == ProcessState::Fermenting,
    };
    static_cast<void>(addEffect(decision, CommandEffect::RunAdjusted));
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
    message->acousticMuted = true;
    ++message->revision;
    ++decision.after.messageRevision;
    static_cast<void>(addEffect(decision, CommandEffect::AcousticMuted));
    return decision;
}

CommandDecision decideFaultReset(const RunCommandState& current,
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
    const auto& evaluation = request.evaluation;
    if (evaluation.faultRevision != current.faultRevision) {
        decision.status = CommandStatus::StaleState;
        return decision;
    }
    if (!evaluation.allowed || evaluation.causeStillActive ||
        !evaluation.safetyChecksPassed || !evaluation.authorizationSatisfied ||
        evaluation.otherBlockingFaultActive ||
        evaluation.rejection != FaultResetRejection::None) {
        decision.status = CommandStatus::SafetyRejected;
        return decision;
    }
    ++decision.after.faultRevision;
    decision.after.criticalSafetyEventPending = false;
    static_cast<void>(addEffect(decision, CommandEffect::FaultResetAuthorized));
    return decision;
}

CommandStatus applyRunCommand(RunCommandState& current,
                              const CommandDecision& decision) {
    if (containsProcessedCommand(current, decision.envelope.id)) {
        return CommandStatus::AlreadyProcessed;
    }
    if (!decision.proposed() ||
        current.commandSequence != decision.before.commandSequence ||
        !equalProcessRuntimeState(current.processState,
                                  decision.before.processState) ||
        current.runRevision != decision.before.runRevision ||
        current.messageRevision != decision.before.messageRevision ||
        current.faultRevision != decision.before.faultRevision ||
        current.activeRunId != decision.before.activeRunId ||
        decision.after.commandSequence != current.commandSequence + 1U) {
        return CommandStatus::StaleState;
    }

    current = decision.after;
    rememberProcessedCommand(current, decision.envelope.id);
    current.lastCommandMonotonicMillis = decision.envelope.monotonicMillis;
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
