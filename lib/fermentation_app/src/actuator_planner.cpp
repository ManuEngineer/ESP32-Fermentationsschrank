#include "actuator_planner.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace fermentation {

namespace {

// Abschnitt 8.3: overflow-sichere Fristpruefung. Ein retrograder Vergleich
// gilt defensiv als nicht erfuellt; die dedizierten Zustandsreferenzen
// werden zusaetzlich unabhaengig davon auf Retrogradheit geprueft (I-8).
[[nodiscard]] bool deadlineReached(std::uint64_t now, std::uint64_t since,
                                    std::uint64_t durationMillis) {
    if (now < since) {
        return false;
    }
    return (now - since) >= durationMillis;
}

[[nodiscard]] double roundHalfUp(double value) { return std::floor(value + 0.5); }

[[nodiscard]] bool isKnownDirection(AbstractControlDirection direction) {
    switch (direction) {
        case AbstractControlDirection::Heating:
        case AbstractControlDirection::Cooling:
        case AbstractControlDirection::Idle:
            return true;
        case AbstractControlDirection::Unknown:
            return false;
    }
    return false;
}

[[nodiscard]] bool isKnownStatus(TemperatureControlStatus status) {
    switch (status) {
        case TemperatureControlStatus::Demand:
        case TemperatureControlStatus::Off:
        case TemperatureControlStatus::Unavailable:
        case TemperatureControlStatus::InvalidInput:
            return true;
    }
    return false;
}

[[nodiscard]] bool isKnownSafetyGateStatus(ActuatorSafetyGateStatus status) {
    switch (status) {
        case ActuatorSafetyGateStatus::Unresolved:
        case ActuatorSafetyGateStatus::Allowed:
        case ActuatorSafetyGateStatus::ImmediateStop:
            return true;
    }
    return false;
}

[[nodiscard]] bool isKnownSensorRole(ControlSensorRole role) {
    switch (role) {
        case ControlSensorRole::Air:
        case ControlSensorRole::Product:
            return true;
    }
    return false;
}

[[nodiscard]] bool isStructurallyValidContext(const ControlRequestContext& context) {
    return isKnownSensorRole(context.controlSensorRole);
}

[[nodiscard]] bool isFiniteUnitQuote(double quote) {
    return std::isfinite(quote) && quote >= 0.0 && quote <= 1.0;
}

[[nodiscard]] bool contextsMatch(const ControlRequestContext& left,
                                  const ControlRequestContext& right) {
    return left.processTransitionSequence == right.processTransitionSequence &&
           left.runRevision == right.runRevision &&
           left.controlSensorRole == right.controlSensorRole;
}

// Abschnitt 6.2 Schritt 2: rein strukturelle Pruefung der neuen #22-
// Evaluation gegen die bereits abschliessende #22-Matrix (Status-/Request-
// Praesenz, Richtung/Quote je Status). Reason-Legalitaet je Status bleibt
// bewusst #22s eigene Verantwortung (siehe issue-22-Plan Abschnitt 7.2);
// classifyActuatorDemand() kann ein "malformed"-Ergebnis strukturell nicht
// zurueckgeben und ist daher kein Ersatz fuer diese Pruefung.
[[nodiscard]] bool isStructurallyValidEvaluation(
    const TemperatureControlResult& evaluation) {
    if (!isKnownStatus(evaluation.status) || !isKnownDirection(evaluation.direction) ||
        !isFiniteUnitQuote(evaluation.timeQuote)) {
        return false;
    }

    const bool requestPresent = evaluation.controlRequest.has_value();
    const bool requiresRequest = evaluation.status == TemperatureControlStatus::Demand ||
                                  evaluation.status == TemperatureControlStatus::Off;
    if (requiresRequest != requestPresent) {
        return false;
    }

    if (evaluation.status == TemperatureControlStatus::Demand) {
        if (evaluation.direction != AbstractControlDirection::Heating &&
            evaluation.direction != AbstractControlDirection::Cooling) {
            return false;
        }
        if (!(evaluation.timeQuote > 0.0)) {
            return false;
        }
    } else {
        if (evaluation.direction != AbstractControlDirection::Idle ||
            evaluation.timeQuote != 0.0) {
            return false;
        }
    }

    if (requestPresent) {
        const ControlRequest& request = *evaluation.controlRequest;
        if (!isKnownDirection(request.direction) || !isFiniteUnitQuote(request.timeQuote)) {
            return false;
        }
        if (request.identity.sequence == 0U) {
            return false;
        }
    }

    return true;
}

}  // namespace

ActuatorPlanner::ActuatorPlanner(ActuatorPlannerParameters parameters)
    : parameters_(std::move(parameters)) {}

const ActuatorPlannerRuntimeState& ActuatorPlanner::state() const { return state_; }

const ActuatorPlannerParameters& ActuatorPlanner::parameters() const {
    return parameters_;
}

AbstractControlDirection ActuatorPlanner::plannedDirection() const {
    return state_.activeWindow.has_value() ? state_.activeWindow->direction
                                            : AbstractControlDirection::Idle;
}

AbstractControlDirection ActuatorPlanner::physicalDirection() const {
    return state_.lastAppliedDirection;
}

void ActuatorPlanner::setPhysicalDirection(AbstractControlDirection next,
                                            std::uint64_t now) {
    if (state_.lastAppliedDirection == next) {
        return;
    }
    if (state_.lastAppliedDirection != AbstractControlDirection::Idle) {
        state_.lastPhysicalDeactivationDirection = state_.lastAppliedDirection;
        state_.lastPhysicalDeactivationAtMonotonicMillis = now;
        state_.currentOnPhaseStartedAtMonotonicMillis.reset();
    }
    if (next != AbstractControlDirection::Idle) {
        state_.currentOnPhaseStartedAtMonotonicMillis = now;
    }
    state_.lastAppliedDirection = next;
}

void ActuatorPlanner::clearPlanningState() {
    state_.acceptedCommand.reset();
    state_.activeWindow.reset();
    state_.accumulator = PulseAccumulator{};
    state_.counterDirectionCandidate.reset();
    state_.counterDirectionObservedSinceMonotonicMillis = 0U;
    state_.counterDirectionConfirmed = false;
}

ActuatorPlanTickResult ActuatorPlanner::buildResult(
    ActuatorPlanStatus status, ActuatorPlanReason reason,
    ActuatorAdmissionOutcome admissionOutcome) const {
    ActuatorPlanTickResult result;
    result.status = status;
    result.reason = reason;
    result.appliedDirection = state_.lastAppliedDirection;
    result.outerFanEnabled = false;   // Abschnitt 10, Commit 4.
    result.innerFanEnabled = false;   // Abschnitt 10, Commit 4.
    result.counterDirectionConfirming =
        state_.counterDirectionCandidate.has_value() && !state_.counterDirectionConfirmed;
    result.admissionOutcome = admissionOutcome;
    result.acceptedCommandSequence =
        (state_.acceptedCommand.has_value() &&
         state_.acceptedCommand->demandClass != ActuatorDemandClass::NoValidRequest)
            ? std::optional<std::uint64_t>(state_.acceptedCommand->sequence)
            : std::nullopt;
    result.watchdogFaultActive = state_.latchedWatchdogFault.has_value();
    return result;
}

ActuatorPlanTickResult ActuatorPlanner::rejectToIdle(const PhaseAOutcome& admission,
                                                      std::uint64_t now,
                                                      ActuatorPlanStatus status,
                                                      ActuatorPlanReason reason) {
    if (physicalDirection() != AbstractControlDirection::Idle) {
        setPhysicalDirection(AbstractControlDirection::Idle, now);
    }
    clearPlanningState();
    const std::optional<std::uint64_t> trustedSequence =
        resolveTrustedSequenceForRejection(admission);
    state_.pendingFeedback =
        trustedSequence.has_value()
            ? std::optional<PendingControlRequestFeedback>(PendingControlRequestFeedback{
                  *trustedSequence, PreviousControlRequestFeedback::Disposition::Rejected})
            : std::nullopt;
    state_.pendingFeedbackUpdateAvailable = true;
    return buildResult(status, reason, admission.admissionOutcome);
}

bool ActuatorPlanner::armingAllowed(AbstractControlDirection direction,
                                     std::uint64_t atMillis) const {
    if (!state_.lastPhysicalDeactivationAtMonotonicMillis.has_value()) {
        return true;  // (a) Erststart seit Konstruktion.
    }
    const std::uint64_t deactivatedAt = *state_.lastPhysicalDeactivationAtMonotonicMillis;
    const bool sameDirection = state_.lastPhysicalDeactivationDirection.has_value() &&
                                *state_.lastPhysicalDeactivationDirection == direction;
    if (sameDirection) {
        return deadlineReached(atMillis, deactivatedAt, parameters_.minimumOffMillis);
    }
    return deadlineReached(atMillis, deactivatedAt, parameters_.minimumOffMillis) &&
           deadlineReached(atMillis, deactivatedAt, parameters_.polarityDeadTimeMillis);
}

void ActuatorPlanner::creditAccumulator(AbstractControlDirection direction,
                                         double quoteMillis) {
    if (state_.accumulator.direction != direction) {
        state_.accumulator.direction = direction;
        state_.accumulator.accumulatedMillis = 0.0;
    }
    state_.accumulator.accumulatedMillis =
        std::min(state_.accumulator.accumulatedMillis + quoteMillis,
                 static_cast<double>(parameters_.pulseAccumulatorCapMillis));
}

void ActuatorPlanner::startFreshWindow(const AcceptedControlCommand& source,
                                        std::uint64_t startMonotonicMillis) {
    ActiveSwitchingWindow window;
    window.startMonotonicMillis = startMonotonicMillis;
    window.direction = source.direction;
    window.sourceRequestSequence = source.sequence;
    window.pulseStartAttempted = false;

    const double requestedOnMillisExact =
        std::clamp(source.timeQuote, 0.0, 1.0) *
        static_cast<double>(parameters_.switchingWindowMillis);

    if (requestedOnMillisExact >= static_cast<double>(parameters_.minimumOnMillis)) {
        window.scheduledOnMillis =
            std::min(static_cast<std::uint64_t>(roundHalfUp(requestedOnMillisExact)),
                     parameters_.switchingWindowMillis);
    } else if (requestedOnMillisExact > 0.0) {
        creditAccumulator(source.direction, requestedOnMillisExact);
        if (state_.accumulator.accumulatedMillis >=
            static_cast<double>(parameters_.minimumOnMillis)) {
            window.scheduledOnMillis = parameters_.minimumOnMillis;
            state_.accumulator.accumulatedMillis -=
                static_cast<double>(parameters_.minimumOnMillis);
        } else {
            window.scheduledOnMillis = 0U;
        }
    } else {
        window.scheduledOnMillis = 0U;
    }

    state_.activeWindow = window;
}

ActuatorPlanner::WindowPhysicalTick ActuatorPlanner::applyWindowPhysicalTick(
    std::uint64_t now) {
    ActiveSwitchingWindow& window = *state_.activeWindow;
    const std::uint64_t elapsedInWindow = now - window.startMonotonicMillis;

    if (window.scheduledOnMillis == 0U) {
        return {false, ActuatorPlanReason::AccumulatingBelowThreshold};
    }

    const bool naturallyActive = elapsedInWindow < window.scheduledOnMillis;

    if (!window.pulseStartAttempted) {
        window.pulseStartAttempted = true;
        if (!naturallyActive) {
            // Fenster erreichte sein natuerliches Ende, bevor je ein
            // Aktivierungsversuch stattfand; kein Nachholen (8.1 Punkt 3).
            return {false, ActuatorPlanReason::WindowPulseMissed};
        }
        if (!armingAllowed(window.direction, window.startMonotonicMillis)) {
            const bool minimumOffOk =
                !state_.lastPhysicalDeactivationAtMonotonicMillis.has_value() ||
                deadlineReached(window.startMonotonicMillis,
                                 *state_.lastPhysicalDeactivationAtMonotonicMillis,
                                 parameters_.minimumOffMillis);
            return {false, minimumOffOk ? ActuatorPlanReason::PolarityDeadTimeHeld
                                         : ActuatorPlanReason::MinimumOffTimeHeld};
        }
        const std::uint64_t remainingNaturalOnMillis =
            window.scheduledOnMillis - elapsedInWindow;
        if (remainingNaturalOnMillis < parameters_.minimumOnMillis) {
            return {false, ActuatorPlanReason::WindowPulseMissed};
        }
        const ActuatorPlanReason triggerReason =
            window.scheduledOnMillis == parameters_.minimumOnMillis
                ? ActuatorPlanReason::MinimumPulseTriggered
                : ActuatorPlanReason::ScheduledWithinWindow;
        return {true, triggerReason};
    }

    if (physicalDirection() == window.direction && naturallyActive) {
        return {true, ActuatorPlanReason::ScheduledWithinWindow};
    }
    if (physicalDirection() == window.direction) {
        // Planmaessiges Fensterende dieses bereits gestarteten Pulses.
        return {false, ActuatorPlanReason::ScheduledWithinWindow};
    }
    return {false, ActuatorPlanReason::WindowPulseMissed};
}

ActuatorPlanTickResult ActuatorPlanner::evaluateHeatingCoolingDemand(
    const AcceptedControlCommand& command, ActuatorAdmissionOutcome admissionOutcome,
    std::uint64_t now) {
    const AbstractControlDirection desired = command.direction;

    // 8.1 Fensterfortschritt (O(1)): ein bestehendes Fenster wird zuerst
    // fortgeschrieben. Ueberschreitet es dabei seine natuerliche Grenze, ist
    // das ein Fensterstart-Ereignis fuer ein gleichgerichtetes Folgefenster
    // oder - bei Gegenrichtung - das endgueltige, folgerlose Ende (Variante
    // B / 8.5: keine Gutschrift aus B fuer das alte Fenster).
    if (state_.activeWindow.has_value()) {
        const ActiveSwitchingWindow& window = *state_.activeWindow;
        const std::uint64_t elapsed = now - window.startMonotonicMillis;
        if (elapsed >= parameters_.switchingWindowMillis) {
            const std::uint64_t windowsElapsed = elapsed / parameters_.switchingWindowMillis;
            const std::uint64_t rebasedStart =
                window.startMonotonicMillis + windowsElapsed * parameters_.switchingWindowMillis;
            const AbstractControlDirection oldDirection = window.direction;
            if (oldDirection == desired) {
                startFreshWindow(command, rebasedStart);
            } else {
                state_.activeWindow.reset();
                state_.accumulator = PulseAccumulator{};
                if (physicalDirection() == oldDirection) {
                    setPhysicalDirection(AbstractControlDirection::Idle, now);
                }
            }
        }
    }

    // Gegenrichtungsbestaetigung (8.5): gefuehrt, solange `desired` von der
    // planungs- oder physisch massgeblichen Richtung abweicht.
    const AbstractControlDirection referenceDirection =
        state_.activeWindow.has_value() ? plannedDirection() : physicalDirection();
    const bool isCounterDirectionCandidate =
        referenceDirection != AbstractControlDirection::Idle && referenceDirection != desired &&
        (command.demandClass == ActuatorDemandClass::NormalDemand ||
         command.demandClass == ActuatorDemandClass::AirLimitReducedDemand) &&
        command.timeQuote >= parameters_.counterDirectionConfirmationQuoteThreshold;

    if (isCounterDirectionCandidate) {
        if (!state_.counterDirectionCandidate.has_value() ||
            *state_.counterDirectionCandidate != desired) {
            state_.counterDirectionCandidate = desired;
            state_.counterDirectionObservedSinceMonotonicMillis = now;
            state_.counterDirectionConfirmed = false;
        }
        state_.counterDirectionConfirmed =
            deadlineReached(now, state_.counterDirectionObservedSinceMonotonicMillis,
                             parameters_.counterDirectionConfirmationDurationMillis);
    } else {
        state_.counterDirectionCandidate.reset();
        state_.counterDirectionObservedSinceMonotonicMillis = 0U;
        state_.counterDirectionConfirmed = false;
    }

    if (state_.activeWindow.has_value() && state_.activeWindow->direction != desired) {
        // Altes Fenster laeuft unter seiner eigenen Quelle grundsaetzlich
        // unveraendert weiter (8.1); ein BESTAETIGTER Richtungswechsel darf
        // es jedoch, wie ein normaler Teardown (N-1/8.5), sofort nach Ende
        // der Mindest-Einschaltzeit beenden, statt auf das natuerliche
        // Fensterende zu warten. Vor Ablauf der Mindest-Einschaltzeit bleibt
        // jede Gegenanforderung - bestaetigt oder nicht - wirkungslos.
        const AbstractControlDirection oldDirection = state_.activeWindow->direction;
        const bool physicallyActiveBefore = physicalDirection() == oldDirection;
        const bool minimumOnSatisfied =
            !physicallyActiveBefore ||
            !state_.currentOnPhaseStartedAtMonotonicMillis.has_value() ||
            deadlineReached(now, *state_.currentOnPhaseStartedAtMonotonicMillis,
                             parameters_.minimumOnMillis);

        if (state_.counterDirectionConfirmed && physicallyActiveBefore && minimumOnSatisfied) {
            setPhysicalDirection(AbstractControlDirection::Idle, now);
            state_.activeWindow.reset();
            state_.accumulator = PulseAccumulator{};
            return buildResult(ActuatorPlanStatus::Idle, ActuatorPlanReason::DirectionChangeApplied,
                                admissionOutcome);
        }

        const WindowPhysicalTick physical = applyWindowPhysicalTick(now);
        if (physical.active) {
            setPhysicalDirection(oldDirection, now);
        } else if (physicalDirection() == oldDirection) {
            setPhysicalDirection(AbstractControlDirection::Idle, now);
        }
        const bool physicallyActiveAfter = physicalDirection() == oldDirection;
        const bool withinMinimumOn =
            physicallyActiveAfter && state_.counterDirectionConfirmed &&
            state_.currentOnPhaseStartedAtMonotonicMillis.has_value() &&
            !deadlineReached(now, *state_.currentOnPhaseStartedAtMonotonicMillis,
                              parameters_.minimumOnMillis);
        return buildResult(
            physicallyActiveAfter ? ActuatorPlanStatus::Active : ActuatorPlanStatus::Idle,
            withinMinimumOn ? ActuatorPlanReason::MinimumOnTimeHeld
                             : ActuatorPlanReason::CounterDirectionConfirming,
            admissionOutcome);
    }

    if (!state_.activeWindow.has_value()) {
        if (referenceDirection != AbstractControlDirection::Idle &&
            referenceDirection != desired && !state_.counterDirectionConfirmed) {
            // Unbestaetigte Gegenrichtung, altes Fenster bereits beendet
            // (N-5d-b): physischer Ausgang bleibt Idle, keine Neuanlage.
            return buildResult(ActuatorPlanStatus::Idle,
                                ActuatorPlanReason::CounterDirectionConfirming, admissionOutcome);
        }
        // Erststart, gleichgerichteter Neustart oder bestaetigte
        // B-Neuanlage.
        startFreshWindow(command, now);
    }

    const WindowPhysicalTick physical = applyWindowPhysicalTick(now);
    if (physical.active) {
        setPhysicalDirection(state_.activeWindow->direction, now);
    } else if (physicalDirection() == state_.activeWindow->direction) {
        setPhysicalDirection(AbstractControlDirection::Idle, now);
    }
    return buildResult(physicalDirection() == AbstractControlDirection::Idle
                            ? ActuatorPlanStatus::Idle
                            : ActuatorPlanStatus::Active,
                        physical.reason, admissionOutcome);
}

std::optional<std::uint64_t> ActuatorPlanner::resolveTrustedSequenceForRejection(
    const PhaseAOutcome& admission) const {
    if (admission.freshTrustedSequence.has_value()) {
        return admission.freshTrustedSequence;
    }
    if (!admission.episodeClosedThisTick && state_.acceptedCommand.has_value() &&
        state_.acceptedCommand->demandClass != ActuatorDemandClass::NoValidRequest) {
        return state_.acceptedCommand->sequence;
    }
    return std::nullopt;
}

bool ActuatorPlanner::hasRetrogradeTimeReference(std::uint64_t now) const {
    const auto retro = [now](const std::optional<std::uint64_t>& reference) {
        return reference.has_value() && now < *reference;
    };
    if (retro(state_.currentOnPhaseStartedAtMonotonicMillis)) return true;
    if (retro(state_.lastPhysicalDeactivationAtMonotonicMillis)) return true;
    if (state_.activeWindow.has_value() && now < state_.activeWindow->startMonotonicMillis) {
        return true;
    }
    if (state_.counterDirectionCandidate.has_value() &&
        now < state_.counterDirectionObservedSinceMonotonicMillis) {
        return true;
    }
    if (retro(state_.lastNewRequestAcceptedAtMonotonicMillis)) return true;
    if (retro(state_.watchdogEpisodeStartedAtMonotonicMillis)) return true;
    if (retro(state_.outerFanDeactivationRequestedAtMonotonicMillis)) return true;
    if (retro(state_.innerFanDeactivationRequestedAtMonotonicMillis)) return true;
    return false;
}

bool ActuatorPlanner::runningWatchdogTripped(const ActuatorPlanTickInput& input) const {
    if (!input.temperatureControlledPhase) {
        return false;
    }
    if (!state_.watchdogEpisodeStartedAtMonotonicMillis.has_value()) {
        return false;
    }
    const std::uint64_t reference = state_.lastNewRequestAcceptedAtMonotonicMillis.value_or(
        *state_.watchdogEpisodeStartedAtMonotonicMillis);
    return deadlineReached(input.nowMonotonicMillis, reference, parameters_.requestWatchdogMillis);
}

ActuatorPlanner::PhaseAOutcome ActuatorPlanner::runPhaseA(const ActuatorPlanTickInput& input) {
    PhaseAOutcome outcome;

    if (!input.newEvaluation.has_value()) {
        return outcome;  // 6.2 Punkt 1: NoCandidate, Phase A endet hier.
    }

    // Neue-Evaluation-Episodengrenze (6.2 Praeambel): das bisherige
    // Feedbackfenster wird immer zuerst geschlossen, unabhaengig davon, was
    // die neue Evaluation ist.
    state_.pendingFeedback.reset();
    state_.pendingFeedbackUpdateAvailable = true;
    outcome.episodeClosedThisTick = true;

    const TemperatureControlResult& evaluation = *input.newEvaluation;

    if (!isStructurallyValidEvaluation(evaluation) ||
        !isStructurallyValidContext(input.currentCanonicalContext)) {
        outcome.admissionOutcome = ActuatorAdmissionOutcome::MalformedCandidate;
        return outcome;  // kein H, kein Kandidat, pendingFeedback bleibt nullopt.
    }

    const bool safetyGateMalformed = !isKnownSafetyGateStatus(input.safetyGate.status);
    const ActuatorDemandClass demandClass = classifyActuatorDemand(evaluation);

    if (demandClass == ActuatorDemandClass::NoValidRequest) {
        outcome.admissionOutcome = safetyGateMalformed
                                        ? ActuatorAdmissionOutcome::MalformedSafetyGate
                                        : ActuatorAdmissionOutcome::Accepted;
        state_.lastNewRequestAcceptedAtMonotonicMillis = input.nowMonotonicMillis;
        AcceptedControlCommand candidate;
        candidate.sequence = 0U;
        candidate.direction = AbstractControlDirection::Idle;
        candidate.timeQuote = 0.0;
        candidate.demandClass = ActuatorDemandClass::NoValidRequest;
        candidate.contextAtAcceptance = input.currentCanonicalContext;
        outcome.candidate = candidate;
        return outcome;
    }

    const ControlRequest& request = *evaluation.controlRequest;
    const std::uint64_t sequence = request.identity.sequence;

    if (state_.lastObservedSequenceHighWatermark.has_value() &&
        sequence <= *state_.lastObservedSequenceHighWatermark) {
        outcome.admissionOutcome = ActuatorAdmissionOutcome::DuplicateOrOldSequence;
        return outcome;
    }

    state_.lastObservedSequenceHighWatermark = sequence;

    if (deadlineReached(input.nowMonotonicMillis, request.identity.createdAtMonotonicMillis,
                         parameters_.requestWatchdogMillis)) {
        outcome.admissionOutcome = ActuatorAdmissionOutcome::StaleOnArrivalWatchdog;
        state_.pendingFeedback = PendingControlRequestFeedback{
            sequence, PreviousControlRequestFeedback::Disposition::Rejected};
        state_.pendingFeedbackUpdateAvailable = true;
        outcome.freshTrustedSequence = sequence;
        return outcome;
    }

    if (!contextsMatch(request.context, input.currentCanonicalContext)) {
        outcome.admissionOutcome = ActuatorAdmissionOutcome::StaleOnArrivalContext;
        state_.pendingFeedback = PendingControlRequestFeedback{
            sequence, PreviousControlRequestFeedback::Disposition::Rejected};
        state_.pendingFeedbackUpdateAvailable = true;
        outcome.freshTrustedSequence = sequence;
        return outcome;
    }

    outcome.admissionOutcome = safetyGateMalformed ? ActuatorAdmissionOutcome::MalformedSafetyGate
                                                    : ActuatorAdmissionOutcome::Accepted;
    state_.lastNewRequestAcceptedAtMonotonicMillis = input.nowMonotonicMillis;
    outcome.freshTrustedSequence = sequence;

    AcceptedControlCommand candidate;
    candidate.sequence = sequence;
    candidate.demandClass = demandClass;
    candidate.contextAtAcceptance = request.context;
    if (demandClass == ActuatorDemandClass::NeutralOff ||
        demandClass == ActuatorDemandClass::AirLimitBlockedOff) {
        candidate.direction = AbstractControlDirection::Idle;
        candidate.timeQuote = 0.0;
    } else {
        candidate.direction = request.direction;
        candidate.timeQuote = request.timeQuote;
    }
    outcome.candidate = candidate;
    return outcome;
}

ActuatorPlanTickResult ActuatorPlanner::runPhaseB(const ActuatorPlanTickInput& input,
                                                   const PhaseAOutcome& admission) {
    const std::uint64_t now = input.nowMonotonicMillis;

    // I-1: MalformedCandidate/MalformedSafetyGate oder ein struktureller
    // Tickwert (Safety-Gate-Enum, aktueller Kontext) ist ungueltig.
    if (admission.admissionOutcome == ActuatorAdmissionOutcome::MalformedCandidate ||
        !isKnownSafetyGateStatus(input.safetyGate.status) ||
        !isStructurallyValidContext(input.currentCanonicalContext)) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::InvalidInput,
                             ActuatorPlanReason::MalformedInput);
    }

    // I-2a/I-2b: Parameterklassifikation.
    const ActuatorPlannerParametersValidation parameterValidation =
        classifyActuatorPlannerParameters(parameters_);
    if (parameterValidation == ActuatorPlannerParametersValidation::Unconfigured) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::Unconfigured,
                             ActuatorPlanReason::NoCommissioning);
    }
    if (parameterValidation == ActuatorPlannerParametersValidation::Invalid) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::InvalidInput,
                             ActuatorPlanReason::InvalidConfiguration);
    }

    // I-3a/I-3b: externes Safety-Gate.
    if (input.safetyGate.status == ActuatorSafetyGateStatus::Unresolved) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::Idle,
                             ActuatorPlanReason::SafetyGateUnresolved);
    }
    if (input.safetyGate.status == ActuatorSafetyGateStatus::ImmediateStop) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::Idle,
                             ActuatorPlanReason::ExternalSafetyOverride);
    }

    // I-4: bereits gelatchter Watchdog-Fehler.
    if (state_.latchedWatchdogFault.has_value()) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::Idle,
                             ActuatorPlanReason::RequestWatchdogFaultLatched);
    }

    // I-5: laufender Watchdog trippt in diesem Tick (8.6).
    if (runningWatchdogTripped(input)) {
        state_.latchedWatchdogFault = ActuatorWatchdogFaultEvidence{
            now, state_.lastObservedSequenceHighWatermark.value_or(0U)};
        return rejectToIdle(admission, now, ActuatorPlanStatus::Idle,
                             ActuatorPlanReason::StaleRequestWatchdog);
    }

    // I-6: neue, explizite NoValidRequest-Evaluation.
    if (admission.candidate.has_value() &&
        admission.candidate->demandClass == ActuatorDemandClass::NoValidRequest) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::Idle,
                             ActuatorPlanReason::NoValidRequest);
    }

    // I-7: gehaltener Request-Kontext ist stale zum aktuellen kanonischen
    // Kontext (nur relevant, wenn dieser Tick keinen frischen Kandidaten
    // liefert, der ihn ohnehin ersetzt).
    if (!admission.candidate.has_value() && state_.acceptedCommand.has_value() &&
        !contextsMatch(state_.acceptedCommand->contextAtAcceptance,
                        input.currentCanonicalContext)) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::Idle,
                             ActuatorPlanReason::StaleRequestContext);
    }

    // I-8: Referenzzeit ist retrograd (8.3).
    if (hasRetrogradeTimeReference(now)) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::InvalidInput,
                             ActuatorPlanReason::TimeInvalid);
    }

    // Klasse N ab hier: der frisch validierte Kandidat (falls vorhanden)
    // wird tatsaechlich als aktuelle Planungsrequest uebernommen.
    if (admission.candidate.has_value()) {
        state_.acceptedCommand = admission.candidate;
    }

    const bool isTeardownRequest = !state_.acceptedCommand.has_value() ||
                                    state_.acceptedCommand->direction ==
                                        AbstractControlDirection::Idle;
    if (isTeardownRequest) {
        const bool airLimitBlocked =
            state_.acceptedCommand.has_value() &&
            state_.acceptedCommand->demandClass == ActuatorDemandClass::AirLimitBlockedOff;
        const bool physicallyActive = physicalDirection() != AbstractControlDirection::Idle;
        const bool withinMinimumOn =
            physicallyActive && state_.currentOnPhaseStartedAtMonotonicMillis.has_value() &&
            !deadlineReached(now, *state_.currentOnPhaseStartedAtMonotonicMillis,
                              parameters_.minimumOnMillis);
        if (withinMinimumOn) {
            // N-1: einzige normale Mindest-On-Halteentscheidung.
            return buildResult(ActuatorPlanStatus::Active, ActuatorPlanReason::MinimumOnTimeHeld,
                                admission.admissionOutcome);
        }
        // N-2: tatsaechlicher normaler Teardown.
        if (physicallyActive) {
            setPhysicalDirection(AbstractControlDirection::Idle, now);
        }
        clearPlanningState();
        return buildResult(ActuatorPlanStatus::Idle,
                            airLimitBlocked ? ActuatorPlanReason::AirLimitBlocked
                                            : ActuatorPlanReason::NeutralIdle,
                            admission.admissionOutcome);
    }

    return evaluateHeatingCoolingDemand(*state_.acceptedCommand, admission.admissionOutcome, now);
}

ActuatorPlanTickResult ActuatorPlanner::tick(const ActuatorPlanTickInput& input) {
    // Abschnitt 6.4: Episodeneintritt wird ausschliesslich ueber diesen Tick
    // erkannt; das Verlassen erfolgt ausschliesslich ueber forceStop().
    if (input.temperatureControlledPhase &&
        !state_.watchdogEpisodeStartedAtMonotonicMillis.has_value()) {
        state_.watchdogEpisodeStartedAtMonotonicMillis = input.nowMonotonicMillis;
    }

    const PhaseAOutcome admission = runPhaseA(input);
    return runPhaseB(input, admission);
}

ActuatorPlanTickResult ActuatorPlanner::forceStop(
    std::uint64_t nowMonotonicMillis, ActuatorFeedbackEpisodeAtStop feedbackEpisodeAtStop) {
    std::optional<std::uint64_t> trustedSequence;
    if (feedbackEpisodeAtStop == ActuatorFeedbackEpisodeAtStop::ExistingEpisodeOpen &&
        state_.acceptedCommand.has_value() &&
        state_.acceptedCommand->demandClass != ActuatorDemandClass::NoValidRequest) {
        trustedSequence = state_.acceptedCommand->sequence;
    }

    if (physicalDirection() != AbstractControlDirection::Idle) {
        setPhysicalDirection(AbstractControlDirection::Idle, nowMonotonicMillis);
    }
    clearPlanningState();
    state_.watchdogEpisodeStartedAtMonotonicMillis.reset();
    state_.lastNewRequestAcceptedAtMonotonicMillis.reset();

    state_.pendingFeedback =
        trustedSequence.has_value()
            ? std::optional<PendingControlRequestFeedback>(PendingControlRequestFeedback{
                  *trustedSequence, PreviousControlRequestFeedback::Disposition::Rejected})
            : std::nullopt;
    state_.pendingFeedbackUpdateAvailable = true;

    return buildResult(ActuatorPlanStatus::Idle, ActuatorPlanReason::NeutralIdle,
                        ActuatorAdmissionOutcome::NoCandidate);
}

void ActuatorPlanner::applyExternalWatchdogFaultReset(std::uint64_t /*nowMonotonicMillis*/) {
    state_.latchedWatchdogFault.reset();
}

}  // namespace fermentation
