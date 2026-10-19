#include "fermentation_ui_projector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "application_lifecycle.hpp"
#include "control_context.hpp"

namespace fermentation {

namespace {

bool hasCanonicalDecisionRequiredMessage(const RunCommandState& state) {
    for (std::size_t i = 0U; i < state.messageCount; ++i) {
        const auto& message = state.messages[i];
        if (!message.active || message.resolved || !message.decisionRequired ||
            message.messageClass != MessageClass::DecisionRequired) {
            continue;
        }
        if (message.code == MessageCode::UserDecisionRequired ||
            message.code == MessageCode::ProductInsertionRequested) {
            return true;
        }
    }
    return false;
}

constexpr std::uint64_t kMaximumJsonSafeInteger = 9007199254740991ULL;

bool phaseHasRunTime(ProcessState state) {
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
        default:
            return false;
    }
}

// Phase elapsed seconds: the monotonic time since the phase was entered plus
// only an exact (lower == upper) prior value tagged for this very phase.
// Anything else is unknown, never a guessed duration.
std::optional<std::uint64_t> phaseElapsedSeconds(
    const RunCommandState& state, std::optional<std::uint64_t> now) {
    if (!phaseHasRunTime(state.processState.state) || !now.has_value() ||
        *now < state.processState.stateEnteredAtMillis) {
        return std::nullopt;
    }
    std::uint64_t prior = 0U;
    if (state.priorBootPhaseElapsed.has_value()) {
        const auto& tagged = *state.priorBootPhaseElapsed;
        if (tagged.taggedState != state.processState.state ||
            !tagged.elapsed.upperBoundSeconds.has_value() ||
            *tagged.elapsed.upperBoundSeconds !=
                tagged.elapsed.lowerBoundSeconds) {
            return std::nullopt;
        }
        prior = tagged.elapsed.lowerBoundSeconds;
    }
    const auto elapsed =
        (*now - state.processState.stateEnteredAtMillis) / 1000U + prior;
    if (elapsed > kMaximumJsonSafeInteger) return std::nullopt;
    return elapsed;
}

// Remaining time only where the process owner itself ends the phase by a
// duration (Fermenting, CoolHolding for a duration); everything else is
// unbounded, unknown or not applicable. Checked arithmetic, no ETA guesses.
RunDetailTime projectRunTime(const RunCommandState& state,
                             std::optional<std::uint64_t> now) {
    RunDetailTime time;
    const auto phase = state.processState.state;
    if (phase == ProcessState::Completed || phase == ProcessState::Standby) {
        time.remainingKind = RunDetailRemainingKind::NotApplicable;
        return time;
    }
    time.elapsedSeconds = phaseElapsedSeconds(state, now);
    if (phase == ProcessState::ManualHolding ||
        (phase == ProcessState::CoolHolding &&
         state.processRunSnapshot.has_value() &&
         state.processRunSnapshot->completionMode ==
             CompletionMode::CoolAndHoldUntilManualStop)) {
        time.remainingKind = RunDetailRemainingKind::Unbounded;
        return time;
    }
    std::optional<std::uint32_t> durationMinutes;
    if (state.processRunSnapshot.has_value()) {
        if (phase == ProcessState::Fermenting) {
            durationMinutes =
                state.processRunSnapshot->fermentationDurationMinutes;
        } else if (phase == ProcessState::CoolHolding &&
                   state.processRunSnapshot->completionMode ==
                       CompletionMode::CoolAndHoldForDuration) {
            durationMinutes = state.processRunSnapshot->holdDurationMinutes;
        }
    }
    if (durationMinutes.has_value() && time.elapsedSeconds.has_value()) {
        const auto total = static_cast<std::uint64_t>(*durationMinutes) * 60U;
        time.remainingSeconds =
            total > *time.elapsedSeconds ? total - *time.elapsedSeconds : 0U;
        time.remainingKind = RunDetailRemainingKind::Timed;
    }
    return time;
}

void projectRunDetail(FermentationHomeView& home, const RunCommandState& state,
                      std::optional<std::uint64_t> now,
                      std::string&& nameBuffer) {
    const ProgramCompletion* completion = nullptr;
    FermentationRunDetail detail;
    if (state.activeProgramRun.has_value()) {
        const auto& source = state.activeProgramRun->snapshot().source;
        detail.sourceKind = programSourceKind(source);
        if (const auto* program = storedProgram(source)) {
            nameBuffer.assign(program->program.name);
            completion = &program->program.completion;
        } else if (const auto* manual = manualTimedSource(source)) {
            completion = &manual->completion;
        }
    } else if (!state.activeManualRun.has_value()) {
        home.programName = std::move(nameBuffer);
        return;
    }
    home.programName = std::move(nameBuffer);
    if (completion != nullptr) {
        RunDetailCompletion value;
        value.mode = completion->mode;
        if (completion->mode != CompletionMode::FinishWithoutCooling) {
            value.coolingTargetCelsius = completion->coolingTargetCelsius;
        }
        if (completion->mode == CompletionMode::CoolAndHoldForDuration) {
            value.holdDurationMinutes = completion->holdDurationMinutes;
        }
        detail.completion = value;
    }
    const auto control = resolveEffectiveControlContext(state);
    if (control.valid && control.target.valid &&
        std::isfinite(control.target.targetCelsius)) {
        detail.targetCelsius = control.target.targetCelsius;
    }
    detail.time = projectRunTime(state, now);
    home.runDetail = detail;
}

}  // namespace

FermentationUiSnapshot FermentationUiProjector::project(
    const FermentationUiProjectionInput& input) {
    FermentationUiSnapshot output;
    projectInto(output, input);
    return output;
}

void FermentationUiProjector::projectInto(
    FermentationUiSnapshot& output,
    const FermentationUiProjectionInput& input) {
    // Keep the vector buffers: move them out, reset the snapshot to its
    // default state, then move the emptied buffers back.
    auto temperatures = std::move(output.temperatures);
    auto messages = std::move(output.messages);
    auto semanticActions = std::move(output.navigation.semanticActions);
    // The run id is the only string-carrying field the application fills in
    // today (R1 allows up to 48 bytes, beyond the small-string buffer), so its
    // capacity is kept as well. Text keys that the application does not
    // populate in uiSnapshot() are not recycled.
    auto activeRunId = std::move(output.home.activeRunId);
    // The frozen program name (<= 96 bytes) is recycled the same way.
    auto programName = std::move(output.home.programName);
    output = FermentationUiSnapshot{};
    programName.clear();
    temperatures.clear();
    messages.clear();
    semanticActions.clear();
    activeRunId.clear();
    output.temperatures = std::move(temperatures);
    output.messages = std::move(messages);
    output.navigation.semanticActions = std::move(semanticActions);
    output.home.activeRunId = std::move(activeRunId);
    output.revisions = input.revisions;
    output.temperatures.reserve(input.temperatures.size());
    for (const auto& source : input.temperatures) {
        output.temperatures.push_back(
            TemperatureView{source.role, source.valueCelsius, source.quality});
    }
    if (input.runState != nullptr) {
        output.messages.reserve(input.runState->messageCount);
        for (std::size_t i = 0U; i < input.runState->messageCount; ++i) {
            output.messages.push_back(MessageView{input.runState->messages[i]});
        }
    }
    output.navigation.semanticActions = input.semanticActions;
    output.status.presentation = input.application.presentation;
    output.status.ready =
        input.application.lifecycleState == ApplicationLifecycleState::Ready;
    output.service.available = input.service.available;
    output.service.confirmationRequired = input.service.confirmationRequired;
    output.service.serviceAuthorizationRequired =
        input.service.serviceAuthorizationRequired;
    output.service.unavailableReason = input.service.unavailableReason;
    output.webAccess = input.webAccess;
    output.factoryReset = input.factoryReset;
    output.network.currentMode = input.network.currentMode;
    output.network.selectionRequired =
        input.network.currentMode == device_platform::NetworkMode::UNSELECTED;
    output.home.primaryAction =
        input.primaryAction.value_or(device_platform::TextKey{});

    if (input.runState != nullptr) {
        const auto& state = *input.runState;
        output.home.processState = state.processState.state;
        output.home.activeRunId = state.activeRunId;
        projectRunDetail(output.home, state, input.monotonicMillis,
                         std::move(programName));
        if (state.activeProgramRun.has_value()) {
            output.home.effectiveValues =
                state.activeProgramRun->effectiveValues();
        } else if (state.activeManualRun.has_value()) {
        }
    }
    output.recovery.canonicalRecoveryDisposition = input.recoveryDisposition;
    output.recovery.persistenceLoadStatus = input.persistenceLoadStatus;
    output.recovery.coordinatorState = input.coordinatorState;
    if (input.recoveryDisposition.has_value()) {
        switch (*input.recoveryDisposition) {
            case RecoveryDisposition::WaitingForTrustedTime:
                output.recovery.mode = RecoveryViewMode::WaitingForTrustedTime;
                break;
            case RecoveryDisposition::CurrentRunRecoverable:
                output.recovery.mode = RecoveryViewMode::CurrentRunRecovered;
                break;
            case RecoveryDisposition::RecoveryRejectedOrFailClosed:
                output.recovery.mode =
                    RecoveryViewMode::RecoveryRejectedOrFailClosed;
                break;
        }
    }
    if (input.runState != nullptr) {
        switch (input.runState->processState.state) {
            case ProcessState::Completed:
                output.recovery.mode = RecoveryViewMode::Completed;
                break;
            case ProcessState::Cooling:
            case ProcessState::CoolHolding:
                output.recovery.mode = RecoveryViewMode::Cooling;
                break;
            default:
                break;
        }
    }
    if (input.persistenceLoadStatus.has_value() &&
        *input.persistenceLoadStatus ==
            RunPersistenceLoadStatus::FallbackRecovered &&
        input.coordinatorState.has_value() &&
        *input.coordinatorState ==
            RunPersistenceCoordinatorState::FallbackRecoveryPending) {
        output.recovery.mode = RecoveryViewMode::FallbackSelectionRequired;
    }
    const auto lifecycle = input.application.lifecycleState;
    if (lifecycle == ApplicationLifecycleState::Initializing) {
        output.home.mode = FermentationHomeMode::Unavailable;
    } else if (lifecycle == ApplicationLifecycleState::ServiceRequired) {
        output.home.mode = FermentationHomeMode::Restricted;
    } else if (input.runState == nullptr) {
        output.home.mode = FermentationHomeMode::Unavailable;
    } else {
        const auto processState = input.runState->processState.state;
        const bool recoveryActive =
            input.recoveryDisposition.has_value() ||
            processState == ProcessState::RecoveryEvaluation ||
            output.recovery.mode == RecoveryViewMode::FallbackSelectionRequired;
        if (recoveryActive) {
            output.home.mode = FermentationHomeMode::Recovery;
        } else {
            const bool decisionRequired =
                hasCanonicalDecisionRequiredMessage(*input.runState);
            switch (processState) {
                case ProcessState::Boot:
                case ProcessState::SafeBoot:
                case ProcessState::Fault:
                case ProcessState::ServiceMode:
                    output.home.mode = FermentationHomeMode::Restricted;
                    break;
                case ProcessState::Completed:
                    output.home.mode = FermentationHomeMode::Completed;
                    break;
                case ProcessState::WaitingForProduct:
                    output.home.mode = FermentationHomeMode::Waiting;
                    break;
                case ProcessState::Preheating:
                case ProcessState::ReachingTarget:
                case ProcessState::QualifyingTarget:
                case ProcessState::Fermenting:
                case ProcessState::Cooling:
                case ProcessState::CoolHolding:
                    output.home.mode = decisionRequired
                                           ? FermentationHomeMode::Waiting
                                           : FermentationHomeMode::ActiveRun;
                    break;
                case ProcessState::ManualHolding:
                    output.home.mode = decisionRequired
                                           ? FermentationHomeMode::Waiting
                                           : FermentationHomeMode::ActiveRun;
                    break;
                case ProcessState::Standby: {
                    output.home.mode = decisionRequired
                                           ? FermentationHomeMode::Waiting
                                           : FermentationHomeMode::Standby;
                    break;
                }
                case ProcessState::RecoveryEvaluation:
                    output.home.mode = FermentationHomeMode::Recovery;
                    break;
            }
        }
    }
    if (input.refreshTracker != nullptr) {
        output.refreshRevision = input.refreshTracker->publish(output);
    }
}

}  // namespace fermentation
