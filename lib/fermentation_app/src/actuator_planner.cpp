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

[[nodiscard]] double roundHalfUp(double value) {
    return std::floor(value + 0.5);
}

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

[[nodiscard]] bool isKnownReason(TemperatureControlReason reason) {
    switch (reason) {
        case TemperatureControlReason::None:
        case TemperatureControlReason::NeutralBand:
        case TemperatureControlReason::Saturated:
        case TemperatureControlReason::AirLimitReduced:
        case TemperatureControlReason::AirLimitBlocked:
        case TemperatureControlReason::NoCommissioning:
        case TemperatureControlReason::SensorUnavailable:
        case TemperatureControlReason::InvalidConfiguration:
        case TemperatureControlReason::InvalidSample:
        case TemperatureControlReason::TimeInvalid:
        case TemperatureControlReason::RequestIdentityExhausted:
            return true;
    }
    return false;
}

[[nodiscard]] bool isKnownAirLimitState(AirLimitState state) {
    switch (state) {
        case AirLimitState::NotApplied:
        case AirLimitState::Unrestricted:
        case AirLimitState::Reduced:
        case AirLimitState::Blocked:
        case AirLimitState::Unavailable:
            return true;
    }
    return false;
}

[[nodiscard]] bool isStructurallyValidContext(
    const ControlRequestContext& context) {
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

[[nodiscard]] int feedbackSeverity(
    PreviousControlRequestFeedback::Disposition disposition) {
    switch (disposition) {
        case PreviousControlRequestFeedback::Disposition::
            NoIntegratorConstraint:
            return 0;
        case PreviousControlRequestFeedback::Disposition::DeferredOrLimited:
            return 1;
        case PreviousControlRequestFeedback::Disposition::Rejected:
            return 2;
    }
    return 2;
}

[[nodiscard]] bool isDownstreamLimitedReason(ActuatorPlanReason reason) {
    switch (reason) {
        case ActuatorPlanReason::MinimumOnTimeHeld:
        case ActuatorPlanReason::MinimumOffTimeHeld:
        case ActuatorPlanReason::PolarityDeadTimeHeld:
        case ActuatorPlanReason::AccumulatingBelowThreshold:
        case ActuatorPlanReason::CounterDirectionConfirming:
        case ActuatorPlanReason::DirectionChangeApplied:
        case ActuatorPlanReason::WindowPulseMissed:
            return true;
        case ActuatorPlanReason::MalformedInput:
        case ActuatorPlanReason::NoCommissioning:
        case ActuatorPlanReason::InvalidConfiguration:
        case ActuatorPlanReason::TimeInvalid:
        case ActuatorPlanReason::SafetyGateUnresolved:
        case ActuatorPlanReason::ExternalSafetyOverride:
        case ActuatorPlanReason::RequestWatchdogFaultLatched:
        case ActuatorPlanReason::StaleRequestWatchdog:
        case ActuatorPlanReason::NoValidRequest:
        case ActuatorPlanReason::StaleRequestContext:
        case ActuatorPlanReason::NeutralIdle:
        case ActuatorPlanReason::AirLimitBlocked:
        case ActuatorPlanReason::MinimumPulseTriggered:
        case ActuatorPlanReason::ScheduledWithinWindow:
            return false;
    }
    return true;
}

// Abschnitt 6.2 Schritt 2 / Abschnitt 7: vollstaendige strukturelle Pruefung
// der neuen #22-Evaluation gegen die bereits abschliessende #22-Matrix
// (issue-22-Plan Abschnitt 7.2): Status/Reason/AirLimitState-Legalitaet,
// Request-Praesenz, Uebereinstimmung von Request- und Ergebnisfeldern sowie
// Request-Kontext-Struktur. classifyActuatorDemand() kann ein "malformed"-
// Ergebnis strukturell nicht zurueckgeben und ist deshalb kein Ersatz fuer
// diese Pruefung; sie muss ihr vorausgehen (Owner-Review ZR1).
[[nodiscard]] bool isStructurallyValidEvaluation(
    const TemperatureControlResult& evaluation) {
    if (!isKnownStatus(evaluation.status) ||
        !isKnownDirection(evaluation.direction) ||
        !isKnownReason(evaluation.reason) ||
        !isKnownAirLimitState(evaluation.airLimitState) ||
        !isFiniteUnitQuote(evaluation.timeQuote)) {
        return false;
    }

    const bool requestPresent = evaluation.controlRequest.has_value();

    switch (evaluation.status) {
        case TemperatureControlStatus::Demand:
            if (!requestPresent) {
                return false;
            }
            if (evaluation.reason != TemperatureControlReason::None &&
                evaluation.reason != TemperatureControlReason::Saturated &&
                evaluation.reason !=
                    TemperatureControlReason::AirLimitReduced) {
                return false;
            }
            if (evaluation.direction != AbstractControlDirection::Heating &&
                evaluation.direction != AbstractControlDirection::Cooling) {
                return false;
            }
            if (!(evaluation.timeQuote > 0.0)) {
                return false;
            }
            break;
        case TemperatureControlStatus::Off:
            if (!requestPresent) {
                return false;
            }
            if (evaluation.reason != TemperatureControlReason::NeutralBand &&
                evaluation.reason !=
                    TemperatureControlReason::AirLimitBlocked) {
                return false;
            }
            if (evaluation.direction != AbstractControlDirection::Idle ||
                evaluation.timeQuote != 0.0) {
                return false;
            }
            break;
        case TemperatureControlStatus::Unavailable:
            if (requestPresent) {
                return false;
            }
            if (evaluation.reason !=
                    TemperatureControlReason::NoCommissioning &&
                evaluation.reason !=
                    TemperatureControlReason::SensorUnavailable) {
                return false;
            }
            if (evaluation.direction != AbstractControlDirection::Idle ||
                evaluation.timeQuote != 0.0) {
                return false;
            }
            break;
        case TemperatureControlStatus::InvalidInput:
            if (requestPresent) {
                return false;
            }
            if (evaluation.reason !=
                    TemperatureControlReason::InvalidConfiguration &&
                evaluation.reason != TemperatureControlReason::InvalidSample &&
                evaluation.reason != TemperatureControlReason::TimeInvalid &&
                evaluation.reason !=
                    TemperatureControlReason::RequestIdentityExhausted) {
                return false;
            }
            if (evaluation.direction != AbstractControlDirection::Idle ||
                evaluation.timeQuote != 0.0) {
                return false;
            }
            break;
    }

    if (!requestPresent) {
        // Owner-Review RZ1: Unavailable/InvalidInput tragen keine Request und
        // damit keine sichtbare ControlSensorRole; ohne sie laesst sich die
        // rollenabhaengige AirLimitState-Kopplung nicht pruefen. Es wird hier
        // bewusst keine neue Fachsemantik erfunden - nur die strukturell
        // unmoegliche Kopplung an Reduced/Blocked bleibt ausgeschlossen, da
        // diese beiden Werte laut #22-Matrix ausschliesslich mit einer
        // vorhandenen Request auftreten.
        return evaluation.airLimitState != AirLimitState::Reduced &&
               evaluation.airLimitState != AirLimitState::Blocked;
    }

    const ControlRequest& request = *evaluation.controlRequest;
    if (!isKnownDirection(request.direction) ||
        !isFiniteUnitQuote(request.timeQuote)) {
        return false;
    }
    if (request.direction != evaluation.direction ||
        request.timeQuote != evaluation.timeQuote) {
        return false;
    }
    if (request.identity.sequence == 0U) {
        return false;
    }
    if (!isStructurallyValidContext(request.context)) {
        return false;
    }

    // Owner-Review RZ1: die AirLimitState-Kopplung ist rollenabhaengig
    // (temperature_control.cpp, issue-22-Plan 7.2). ControlSensorRole::Air
    // steuert die Luft direkt; Luftbegrenzung ist dort strukturell nicht
    // anwendbar und traegt immer NotApplied. ControlSensorRole::Product
    // unterliegt der eigentlichen Luftbegrenzung.
    switch (request.context.controlSensorRole) {
        case ControlSensorRole::Air:
            if (evaluation.reason ==
                    TemperatureControlReason::AirLimitReduced ||
                evaluation.reason ==
                    TemperatureControlReason::AirLimitBlocked) {
                return false;
            }
            if (evaluation.airLimitState != AirLimitState::NotApplied) {
                return false;
            }
            break;
        case ControlSensorRole::Product:
            if (evaluation.status == TemperatureControlStatus::Demand) {
                if (evaluation.reason ==
                    TemperatureControlReason::AirLimitReduced) {
                    if (evaluation.airLimitState != AirLimitState::Reduced) {
                        return false;
                    }
                } else if (evaluation.airLimitState !=
                           AirLimitState::Unrestricted) {
                    return false;
                }
            } else {
                // Off (die einzige verbleibende requestPresent-Moeglichkeit).
                if (evaluation.reason ==
                    TemperatureControlReason::AirLimitBlocked) {
                    if (evaluation.airLimitState != AirLimitState::Blocked) {
                        return false;
                    }
                } else if (evaluation.airLimitState !=
                           AirLimitState::Unrestricted) {
                    return false;
                }
            }
            break;
    }

    return true;
}

}  // namespace

ActuatorPlanner::ActuatorPlanner(ActuatorPlannerParameters parameters)
    : parameters_(std::move(parameters)) {}

const ActuatorPlannerRuntimeState& ActuatorPlanner::state() const {
    return state_;
}

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
        // The outer fan follows the physical Peltier transition, not the
        // still-live planning window. Every real deactivation starts or
        // refreshes the mandatory post-run anchor.
        state_.outerFanActive = true;
        state_.outerFanDeactivationRequestedAtMonotonicMillis = now;
    }
    if (next != AbstractControlDirection::Idle) {
        state_.currentOnPhaseStartedAtMonotonicMillis = now;
        state_.outerFanActive = true;
        state_.outerFanDeactivationRequestedAtMonotonicMillis.reset();
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

void ActuatorPlanner::mergeFeedback(
    std::uint64_t sequence,
    PreviousControlRequestFeedback::Disposition disposition) {
    if (sequence == 0U) return;

    if (!state_.pendingFeedback.has_value() ||
        state_.pendingFeedback->sequence != sequence) {
        state_.pendingFeedback =
            PendingControlRequestFeedback{sequence, disposition};
        state_.pendingFeedbackUpdateAvailable = true;
        return;
    }

    if (feedbackSeverity(disposition) >
        feedbackSeverity(state_.pendingFeedback->disposition)) {
        state_.pendingFeedback->disposition = disposition;
        state_.pendingFeedbackUpdateAvailable = true;
    }
}

void ActuatorPlanner::mergeFeedbackForDemand(
    const AcceptedControlCommand& command, ActuatorPlanReason reason) {
    // Owner-Review F1: command may be an old acceptedCommand still governed
    // physically (minimum-on, teardown) after its own feedback episode
    // already closed via a newer evaluation. Only the sequence currently
    // owning the open episode may still mutate pendingFeedback; a foreign or
    // already-closed subject is silently skipped so it cannot resurrect a
    // stale disposition over the current episode's subject.
    if (!state_.feedbackEpisodeSubjectSequence.has_value() ||
        *state_.feedbackEpisodeSubjectSequence != command.sequence) {
        return;
    }
    mergeFeedback(
        command.sequence,
        isDownstreamLimitedReason(reason)
            ? PreviousControlRequestFeedback::Disposition::DeferredOrLimited
            : PreviousControlRequestFeedback::Disposition::
                  NoIntegratorConstraint);
}

void ActuatorPlanner::updateFanState(std::uint64_t now,
                                     bool temperatureControlledPhase) {
    if (state_.lastAppliedDirection != AbstractControlDirection::Idle) {
        state_.outerFanActive = true;
        state_.outerFanDeactivationRequestedAtMonotonicMillis.reset();
    } else if (state_.outerFanActive &&
               state_.outerFanDeactivationRequestedAtMonotonicMillis
                   .has_value() &&
               deadlineReached(
                   now, *state_.outerFanDeactivationRequestedAtMonotonicMillis,
                   parameters_.outerFanPostRunMillis)) {
        state_.outerFanActive = false;
    }

    if (temperatureControlledPhase) {
        state_.innerFanActive = true;
        state_.innerFanDeactivationRequestedAtMonotonicMillis.reset();
    } else {
        if (state_.innerFanActive &&
            !state_.innerFanDeactivationRequestedAtMonotonicMillis
                 .has_value()) {
            state_.innerFanDeactivationRequestedAtMonotonicMillis = now;
        }
        if (state_.innerFanActive &&
            state_.innerFanDeactivationRequestedAtMonotonicMillis.has_value() &&
            deadlineReached(
                now, *state_.innerFanDeactivationRequestedAtMonotonicMillis,
                parameters_.innerFanPostRunMillis)) {
            state_.innerFanActive = false;
        }
    }
}

void ActuatorPlanner::copyFanStateToResult(
    ActuatorPlanTickResult& result) const {
    result.outerFanEnabled = state_.outerFanActive;
    result.innerFanEnabled = state_.innerFanActive;
}

ActuatorPlanTickResult ActuatorPlanner::buildResult(
    ActuatorPlanStatus status, ActuatorPlanReason reason,
    ActuatorAdmissionOutcome admissionOutcome) const {
    ActuatorPlanTickResult result;
    result.status = status;
    result.reason = reason;
    result.appliedDirection = state_.lastAppliedDirection;
    copyFanStateToResult(result);
    result.counterDirectionConfirming =
        state_.counterDirectionCandidate.has_value() &&
        !state_.counterDirectionConfirmed;
    result.admissionOutcome = admissionOutcome;
    result.acceptedCommandSequence =
        (state_.acceptedCommand.has_value() &&
         state_.acceptedCommand->demandClass !=
             ActuatorDemandClass::NoValidRequest)
            ? std::optional<std::uint64_t>(state_.acceptedCommand->sequence)
            : std::nullopt;
    result.watchdogFaultActive = state_.latchedWatchdogFault.has_value();
    return result;
}

ActuatorPlanTickResult ActuatorPlanner::rejectToIdle(
    const PhaseAOutcome& admission, std::uint64_t now,
    ActuatorPlanStatus status, ActuatorPlanReason reason) {
    // Owner-Review ZR4: das Feedbacksubjekt muss VOR jeder Planungs-
    // bereinigung aufgeloest werden, da resolveTrustedSequenceForRejection()
    // ein noch gehaltenes acceptedCommand liest.
    const std::optional<std::uint64_t> trustedSequence =
        resolveTrustedSequenceForRejection(admission);
    if (physicalDirection() != AbstractControlDirection::Idle) {
        setPhysicalDirection(AbstractControlDirection::Idle, now);
    }
    clearPlanningState();
    if (trustedSequence.has_value()) {
        mergeFeedback(*trustedSequence,
                      PreviousControlRequestFeedback::Disposition::Rejected);
    } else {
        state_.pendingFeedback.reset();
        state_.pendingFeedbackUpdateAvailable = true;
    }
    // Owner-Review F1: the immediate fail-closed teardown terminates the
    // episode either way (Rejected or no subject); no later tick may still
    // treat this sequence as an open feedback subject.
    state_.feedbackEpisodeSubjectSequence.reset();
    return buildResult(status, reason, admission.admissionOutcome);
}

bool ActuatorPlanner::armingAllowed(AbstractControlDirection direction,
                                    std::uint64_t atMillis) const {
    if (!state_.lastPhysicalDeactivationAtMonotonicMillis.has_value()) {
        return true;  // (a) Erststart seit Konstruktion.
    }
    const std::uint64_t deactivatedAt =
        *state_.lastPhysicalDeactivationAtMonotonicMillis;
    const bool sameDirection =
        state_.lastPhysicalDeactivationDirection.has_value() &&
        *state_.lastPhysicalDeactivationDirection == direction;
    if (sameDirection) {
        return deadlineReached(atMillis, deactivatedAt,
                               parameters_.minimumOffMillis);
    }
    return deadlineReached(atMillis, deactivatedAt,
                           parameters_.minimumOffMillis) &&
           deadlineReached(atMillis, deactivatedAt,
                           parameters_.polarityDeadTimeMillis);
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

    if (requestedOnMillisExact >=
        static_cast<double>(parameters_.minimumOnMillis)) {
        window.scheduledOnMillis = std::min(
            static_cast<std::uint64_t>(roundHalfUp(requestedOnMillisExact)),
            parameters_.switchingWindowMillis);
        window.minimumPulseFromAccumulator = false;
    } else if (requestedOnMillisExact > 0.0) {
        creditAccumulator(source.direction, requestedOnMillisExact);
        if (state_.accumulator.accumulatedMillis >=
            static_cast<double>(parameters_.minimumOnMillis)) {
            window.scheduledOnMillis = parameters_.minimumOnMillis;
            state_.accumulator.accumulatedMillis -=
                static_cast<double>(parameters_.minimumOnMillis);
            window.minimumPulseFromAccumulator = true;
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
                deadlineReached(
                    window.startMonotonicMillis,
                    *state_.lastPhysicalDeactivationAtMonotonicMillis,
                    parameters_.minimumOffMillis);
            return {false, minimumOffOk
                               ? ActuatorPlanReason::PolarityDeadTimeHeld
                               : ActuatorPlanReason::MinimumOffTimeHeld};
        }
        const std::uint64_t remainingNaturalOnMillis =
            window.scheduledOnMillis - elapsedInWindow;
        if (remainingNaturalOnMillis < parameters_.minimumOnMillis) {
            return {false, ActuatorPlanReason::WindowPulseMissed};
        }
        const ActuatorPlanReason triggerReason =
            window.minimumPulseFromAccumulator
                ? ActuatorPlanReason::MinimumPulseTriggered
                : ActuatorPlanReason::ScheduledWithinWindow;
        window.pulseStartedSuccessfully = true;
        return {true, triggerReason};
    }

    // Owner-Review RZ4: der einzige Aktivierungsversuch ist bereits erfolgt.
    // pulseStartedSuccessfully unterscheidet eindeutig den planmaessigen
    // Off-Anteil eines tatsaechlich gestarteten Pulses (weiterhin
    // ScheduledWithinWindow, physisch Idle) von einem Puls, der nie
    // erfolgreich gestartet wurde (WindowPulseMissed) - unabhaengig davon,
    // wie viele weitere Ticks seither vergangen sind.
    if (window.pulseStartedSuccessfully) {
        return {naturallyActive, ActuatorPlanReason::ScheduledWithinWindow};
    }
    return {false, ActuatorPlanReason::WindowPulseMissed};
}

ActuatorPlanTickResult ActuatorPlanner::evaluateHeatingCoolingDemand(
    const AcceptedControlCommand& command,
    ActuatorAdmissionOutcome admissionOutcome, std::uint64_t now) {
    const AbstractControlDirection desired = command.direction;

    // Referenzrichtung VOR jeder Fenstermutation dieses Ticks (Owner-Review
    // ZR3, verschaerft durch RZ3): entscheidet, ob `desired` in DIESEM Tick
    // erstmals als abweichende Gegenrichtung gilt. Darf nicht durch die
    // nachfolgende Fensterfortschreibung (die das alte Fenster in DIESEM
    // Tick loeschen kann) verfaelscht werden. Existiert weder Fenster noch
    // aktive Physik, bleibt lastPhysicalDeactivationDirection die Referenz:
    // eine Rueckkehr zu ihr ist ein gleichgerichteter Neustart, jede andere
    // Richtung bleibt eine Gegenrichtung - auch nachdem der physische
    // Ausgang laengst Idle geworden ist. Nur bei nullopt (Erststart seit
    // Konstruktion) gibt es keine Referenz.
    const AbstractControlDirection referenceDirectionAtTickStart =
        state_.activeWindow.has_value()
            ? plannedDirection()
            : (physicalDirection() != AbstractControlDirection::Idle
                   ? physicalDirection()
                   : state_.lastPhysicalDeactivationDirection.value_or(
                         AbstractControlDirection::Idle));

    // 8.1 Fensterfortschritt (O(1)): ein bestehendes Fenster wird zuerst
    // fortgeschrieben. Ueberschreitet es dabei seine natuerliche Grenze, ist
    // das ein Fensterstart-Ereignis fuer ein gleichgerichtetes Folgefenster
    // oder - bei Gegenrichtung - das endgueltige, folgerlose Ende (Variante
    // B / 8.5: keine Gutschrift aus B fuer das alte Fenster).
    if (state_.activeWindow.has_value()) {
        const ActiveSwitchingWindow& window = *state_.activeWindow;
        const std::uint64_t elapsed = now - window.startMonotonicMillis;
        if (elapsed >= parameters_.switchingWindowMillis) {
            const std::uint64_t windowsElapsed =
                elapsed / parameters_.switchingWindowMillis;
            const std::uint64_t rebasedStart =
                window.startMonotonicMillis +
                windowsElapsed * parameters_.switchingWindowMillis;
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

    // Gegenrichtungsbestaetigung (8.5, Owner-Review ZR3): ein bereits
    // verfolgter Kandidat bleibt bestehen, solange `desired` weiterhin auf
    // ihn zeigt und die Eignungskriterien erfuellt sind - unabhaengig davon,
    // ob das alte Fenster/die alte Physik in DIESEM Tick bereits
    // verschwunden ist. Die Bestaetigung laeuft so ueber beliebig viele
    // Ticks/Fenstergrenzen weiter.
    const bool eligibleForCounterDirection =
        (command.demandClass == ActuatorDemandClass::NormalDemand ||
         command.demandClass == ActuatorDemandClass::AirLimitReducedDemand) &&
        command.timeQuote >=
            parameters_.counterDirectionConfirmationQuoteThreshold;
    const bool isCounterDirectionCandidate =
        eligibleForCounterDirection &&
        ((referenceDirectionAtTickStart != AbstractControlDirection::Idle &&
          referenceDirectionAtTickStart != desired) ||
         (state_.counterDirectionCandidate.has_value() &&
          *state_.counterDirectionCandidate == desired));

    if (isCounterDirectionCandidate) {
        if (!state_.counterDirectionCandidate.has_value() ||
            *state_.counterDirectionCandidate != desired) {
            state_.counterDirectionCandidate = desired;
            state_.counterDirectionObservedSinceMonotonicMillis = now;
            state_.counterDirectionConfirmed = false;
        }
        state_.counterDirectionConfirmed = deadlineReached(
            now, state_.counterDirectionObservedSinceMonotonicMillis,
            parameters_.counterDirectionConfirmationDurationMillis);
    } else {
        state_.counterDirectionCandidate.reset();
        state_.counterDirectionObservedSinceMonotonicMillis = 0U;
        state_.counterDirectionConfirmed = false;
    }

    if (state_.activeWindow.has_value() &&
        state_.activeWindow->direction != desired) {
        // Altes Fenster laeuft unter seiner eigenen Quelle grundsaetzlich
        // unveraendert weiter (8.1); ein BESTAETIGTER Richtungswechsel darf
        // es jedoch, wie ein normaler Teardown (N-1/8.5), sofort nach Ende
        // der Mindest-Einschaltzeit beenden, statt auf das natuerliche
        // Fensterende zu warten. Vor Ablauf der Mindest-Einschaltzeit bleibt
        // jede Gegenanforderung - bestaetigt oder nicht - wirkungslos.
        const AbstractControlDirection oldDirection =
            state_.activeWindow->direction;
        const bool physicallyActiveBefore = physicalDirection() == oldDirection;
        const bool minimumOnSatisfied =
            !physicallyActiveBefore ||
            !state_.currentOnPhaseStartedAtMonotonicMillis.has_value() ||
            deadlineReached(now, *state_.currentOnPhaseStartedAtMonotonicMillis,
                            parameters_.minimumOnMillis);

        if (state_.counterDirectionConfirmed) {
            if (physicallyActiveBefore) {
                if (!minimumOnSatisfied) {
                    const WindowPhysicalTick physical =
                        applyWindowPhysicalTick(now);
                    if (physical.active) {
                        setPhysicalDirection(oldDirection, now);
                    }
                    mergeFeedbackForDemand(
                        command, ActuatorPlanReason::MinimumOnTimeHeld);
                    return buildResult(ActuatorPlanStatus::Active,
                                       ActuatorPlanReason::MinimumOnTimeHeld,
                                       admissionOutcome);
                }
                setPhysicalDirection(AbstractControlDirection::Idle, now);
                state_.activeWindow.reset();
                state_.accumulator = PulseAccumulator{};
                mergeFeedbackForDemand(
                    command, ActuatorPlanReason::DirectionChangeApplied);
                return buildResult(ActuatorPlanStatus::Idle,
                                   ActuatorPlanReason::DirectionChangeApplied,
                                   admissionOutcome);
            }

            // Die alte physische Richtung ist bereits im Window-Off-Anteil
            // beendet. Nach der Bestaetigung wird ihr Snapshot sofort
            // verworfen; die nachfolgende Wartephase prueft nur noch die
            // reale Minimum-Off-/Totzeit ab dem letzten Active -> Idle.
            state_.activeWindow.reset();
            state_.accumulator = PulseAccumulator{};
        } else {
            const WindowPhysicalTick physical = applyWindowPhysicalTick(now);
            if (physical.active) {
                setPhysicalDirection(oldDirection, now);
            } else if (physicalDirection() == oldDirection) {
                setPhysicalDirection(AbstractControlDirection::Idle, now);
            }
            const bool physicallyActiveAfter =
                physicalDirection() == oldDirection;
            const bool withinMinimumOn =
                physicallyActiveAfter && state_.counterDirectionConfirmed &&
                state_.currentOnPhaseStartedAtMonotonicMillis.has_value() &&
                !deadlineReached(now,
                                 *state_.currentOnPhaseStartedAtMonotonicMillis,
                                 parameters_.minimumOnMillis);
            mergeFeedbackForDemand(
                command, withinMinimumOn
                             ? ActuatorPlanReason::MinimumOnTimeHeld
                             : ActuatorPlanReason::CounterDirectionConfirming);
            return buildResult(
                physicallyActiveAfter ? ActuatorPlanStatus::Active
                                      : ActuatorPlanStatus::Idle,
                withinMinimumOn
                    ? ActuatorPlanReason::MinimumOnTimeHeld
                    : ActuatorPlanReason::CounterDirectionConfirming,
                admissionOutcome);
        }
    }

    if (!state_.activeWindow.has_value()) {
        if (state_.counterDirectionCandidate.has_value() &&
            *state_.counterDirectionCandidate == desired) {
            if (!state_.counterDirectionConfirmed) {
                // Unbestaetigte Gegenrichtung, altes Fenster/alte Physik
                // bereits beendet (N-5d-b): physischer Ausgang bleibt Idle,
                // keine Neuanlage; die Buchfuehrung bleibt oben erhalten.
                mergeFeedbackForDemand(
                    command, ActuatorPlanReason::CounterDirectionConfirming);
                return buildResult(
                    ActuatorPlanStatus::Idle,
                    ActuatorPlanReason::CounterDirectionConfirming,
                    admissionOutcome);
            }
            // Owner-Review RZ2: eine bestaetigte Gegenrichtung wird erst neu
            // geplant, sobald Minimum-Off und - bei echtem Richtungswechsel -
            // Polaritaetstotzeit ab dem realen alten Active -> Idle erfuellt
            // sind. Solange eine Frist offen ist, bleibt der Ausgang Idle und
            // die Bestaetigung bleibt erhalten (kein "versuchter" Fensterbau
            // als Ersatz fuer eine echte Neuanlage).
            if (!armingAllowed(desired, now)) {
                const bool minimumOffOk =
                    !state_.lastPhysicalDeactivationAtMonotonicMillis
                         .has_value() ||
                    deadlineReached(
                        now, *state_.lastPhysicalDeactivationAtMonotonicMillis,
                        parameters_.minimumOffMillis);
                mergeFeedbackForDemand(
                    command, minimumOffOk
                                 ? ActuatorPlanReason::PolarityDeadTimeHeld
                                 : ActuatorPlanReason::MinimumOffTimeHeld);
                return buildResult(
                    ActuatorPlanStatus::Idle,
                    minimumOffOk ? ActuatorPlanReason::PolarityDeadTimeHeld
                                 : ActuatorPlanReason::MinimumOffTimeHeld,
                    admissionOutcome);
            }
            startFreshWindow(command, now);
            // Erfolgreiche B-Neuanlage: Buchfuehrung wird erst jetzt
            // geloescht.
            state_.counterDirectionCandidate.reset();
            state_.counterDirectionObservedSinceMonotonicMillis = 0U;
            state_.counterDirectionConfirmed = false;
        } else if (referenceDirectionAtTickStart !=
                       AbstractControlDirection::Idle &&
                   referenceDirectionAtTickStart != desired) {
            // Owner-Review RZ3: `desired` weicht von der zuletzt realen
            // Richtung ab, hat aber (noch) nicht die Bestaetigungsschwelle
            // erreicht - sonst waere sie oben bereits als Kandidat getrackt.
            // Kein automatischer Erststart, keine Energie.
            mergeFeedbackForDemand(
                command, ActuatorPlanReason::CounterDirectionConfirming);
            return buildResult(ActuatorPlanStatus::Idle,
                               ActuatorPlanReason::CounterDirectionConfirming,
                               admissionOutcome);
        } else {
            // Erststart seit Konstruktion oder gleichgerichteter Neustart.
            startFreshWindow(command, now);
        }
    }

    // Owner-Review ZR7: eine neue gleichgerichtete Request, deren eigene
    // Quote unter minimumOnMillis liegt, traegt ihre eigene Governance, auch
    // wenn sie physisch weiterhin unter dem (fremden, aelteren) laufenden
    // Fenster mitlaeuft (Variante B). Nur relevant, wenn das aktive Fenster
    // NICHT aus `command` selbst stammt; ein soeben erzeugtes/rebasiertes
    // Fenster traegt bereits `command.sequence` und ist davon nicht
    // betroffen.
    const bool sameDirectionMidWindowMismatch =
        state_.activeWindow->sourceRequestSequence != command.sequence;
    const double ownRequestedOnMillisExact =
        std::clamp(command.timeQuote, 0.0, 1.0) *
        static_cast<double>(parameters_.switchingWindowMillis);
    const bool ownQuoteBelowMinimum =
        ownRequestedOnMillisExact <
        static_cast<double>(parameters_.minimumOnMillis);

    const WindowPhysicalTick physical = applyWindowPhysicalTick(now);
    if (physical.active) {
        setPhysicalDirection(state_.activeWindow->direction, now);
    } else if (physicalDirection() == state_.activeWindow->direction) {
        setPhysicalDirection(AbstractControlDirection::Idle, now);
    }
    const ActuatorPlanReason reason =
        (sameDirectionMidWindowMismatch && ownQuoteBelowMinimum)
            ? ActuatorPlanReason::AccumulatingBelowThreshold
            : physical.reason;
    mergeFeedbackForDemand(command, reason);
    return buildResult(physicalDirection() == AbstractControlDirection::Idle
                           ? ActuatorPlanStatus::Idle
                           : ActuatorPlanStatus::Active,
                       reason, admissionOutcome);
}

std::optional<std::uint64_t>
ActuatorPlanner::resolveTrustedSequenceForRejection(
    const PhaseAOutcome& admission) const {
    if (admission.freshTrustedActiveSequence.has_value()) {
        return admission.freshTrustedActiveSequence;
    }
    // Owner-Review ZR5 gilt hier identisch: ein gehaltenes OFF-acceptedCommand
    // (NeutralOff/AirLimitBlockedOff) ist kein Feedbacksubjekt.
    if (!admission.episodeClosedThisTick &&
        state_.acceptedCommand.has_value() &&
        (state_.acceptedCommand->demandClass ==
             ActuatorDemandClass::NormalDemand ||
         state_.acceptedCommand->demandClass ==
             ActuatorDemandClass::AirLimitReducedDemand)) {
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
    if (state_.activeWindow.has_value() &&
        now < state_.activeWindow->startMonotonicMillis) {
        return true;
    }
    if (state_.counterDirectionCandidate.has_value() &&
        now < state_.counterDirectionObservedSinceMonotonicMillis) {
        return true;
    }
    if (retro(state_.lastNewRequestAcceptedAtMonotonicMillis)) return true;
    if (retro(state_.watchdogEpisodeStartedAtMonotonicMillis)) return true;
    if (retro(state_.outerFanDeactivationRequestedAtMonotonicMillis))
        return true;
    if (retro(state_.innerFanDeactivationRequestedAtMonotonicMillis))
        return true;
    return false;
}

bool ActuatorPlanner::runningWatchdogTripped(
    const ActuatorPlanTickInput& input) const {
    if (!input.temperatureControlledPhase) {
        return false;
    }
    if (!state_.watchdogEpisodeStartedAtMonotonicMillis.has_value()) {
        return false;
    }
    const std::uint64_t reference =
        state_.lastNewRequestAcceptedAtMonotonicMillis.value_or(
            *state_.watchdogEpisodeStartedAtMonotonicMillis);
    return deadlineReached(input.nowMonotonicMillis, reference,
                           parameters_.requestWatchdogMillis);
}

void ActuatorPlanner::openFeedbackEpisode(
    std::optional<std::uint64_t> subjectSequence) {
    // Owner-Review F1: closes whatever the previous episode's subject was
    // and opens the new one (or none) atomically, so a stale
    // acceptedCommand from the old subject can never be mistaken for the
    // new one on a subsequent tick.
    state_.feedbackEpisodeSubjectSequence = subjectSequence;
    state_.pendingFeedback.reset();
    state_.pendingFeedbackUpdateAvailable = true;
}

ActuatorPlanner::PhaseAOutcome ActuatorPlanner::runPhaseA(
    const ActuatorPlanTickInput& input) {
    PhaseAOutcome outcome;

    if (!input.newEvaluation.has_value()) {
        return outcome;  // 6.2 Punkt 1: NoCandidate, Phase A endet hier.
    }

    const TemperatureControlResult& evaluation = *input.newEvaluation;

    if (!isStructurallyValidEvaluation(evaluation) ||
        !isStructurallyValidContext(input.currentCanonicalContext)) {
        // 6.2 Punkt 2: eine strukturell unsichere Identitaet schliesst das
        // Feedbackfenster fail-closed, ohne ein neues Subjekt zu eroeffnen.
        openFeedbackEpisode(std::nullopt);
        outcome.episodeClosedThisTick = true;
        outcome.admissionOutcome = ActuatorAdmissionOutcome::MalformedCandidate;
        return outcome;  // kein H, kein Kandidat.
    }

    const bool safetyGateMalformed =
        !isKnownSafetyGateStatus(input.safetyGate.status);
    const ActuatorDemandClass demandClass = classifyActuatorDemand(evaluation);

    if (demandClass == ActuatorDemandClass::NoValidRequest) {
        // 6.2 Punkt 4: eine lebende, aber requestlose Auswertung schliesst
        // das alte Fenster ebenfalls, ohne selbst ein Subjekt zu eroeffnen.
        openFeedbackEpisode(std::nullopt);
        outcome.episodeClosedThisTick = true;
        outcome.admissionOutcome =
            safetyGateMalformed ? ActuatorAdmissionOutcome::MalformedSafetyGate
                                : ActuatorAdmissionOutcome::Accepted;
        state_.lastNewRequestAcceptedAtMonotonicMillis =
            input.nowMonotonicMillis;
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
    // Owner-Review ZR5: #22 oeffnet ein Feedbackfenster ausschliesslich fuer
    // eine aktive Heating/Cooling-Request (issue-22-Plan 8.1/8.2); eine
    // gueltige OFF-Request erhaelt niemals ein Sequence-Feedback, auch nicht
    // bei Stale-on-arrival oder korruptem externem Safety-Gate. Die Replay-/
    // High-Watermark-Buchfuehrung bleibt fuer OFF unveraendert.
    const bool isActiveHeatingCoolingDemand =
        demandClass == ActuatorDemandClass::NormalDemand ||
        demandClass == ActuatorDemandClass::AirLimitReducedDemand;

    if (state_.lastObservedSequenceHighWatermark.has_value() &&
        sequence <= *state_.lastObservedSequenceHighWatermark) {
        // 6.2 Punkt 5 / 9.2: ein Replay beruehrt weder Zeitbasis noch ein
        // bestehendes Feedbackfenster; das Subjekt bleibt exakt so, wie es
        // vor diesem Tick war (Owner-Review F1).
        outcome.admissionOutcome =
            ActuatorAdmissionOutcome::DuplicateOrOldSequence;
        return outcome;
    }

    // Ab hier ist die Sequence strukturell neu und vertrauenswuerdig: das
    // Feedbackfenster schliesst in jedem Fall (6.2 Praeambel), auch wenn
    // unten kein neues Subjekt eroeffnet wird.
    openFeedbackEpisode(std::nullopt);
    outcome.episodeClosedThisTick = true;
    state_.lastObservedSequenceHighWatermark = sequence;

    if (deadlineReached(input.nowMonotonicMillis,
                        request.identity.createdAtMonotonicMillis,
                        parameters_.requestWatchdogMillis)) {
        outcome.admissionOutcome =
            ActuatorAdmissionOutcome::StaleOnArrivalWatchdog;
        if (isActiveHeatingCoolingDemand) {
            state_.feedbackEpisodeSubjectSequence = sequence;
            state_.pendingFeedback = PendingControlRequestFeedback{
                sequence,
                PreviousControlRequestFeedback::Disposition::Rejected};
            state_.pendingFeedbackUpdateAvailable = true;
            outcome.freshTrustedActiveSequence = sequence;
        }
        return outcome;
    }

    if (!contextsMatch(request.context, input.currentCanonicalContext)) {
        outcome.admissionOutcome =
            ActuatorAdmissionOutcome::StaleOnArrivalContext;
        if (isActiveHeatingCoolingDemand) {
            state_.feedbackEpisodeSubjectSequence = sequence;
            state_.pendingFeedback = PendingControlRequestFeedback{
                sequence,
                PreviousControlRequestFeedback::Disposition::Rejected};
            state_.pendingFeedbackUpdateAvailable = true;
            outcome.freshTrustedActiveSequence = sequence;
        }
        return outcome;
    }

    outcome.admissionOutcome =
        safetyGateMalformed ? ActuatorAdmissionOutcome::MalformedSafetyGate
                            : ActuatorAdmissionOutcome::Accepted;
    state_.lastNewRequestAcceptedAtMonotonicMillis = input.nowMonotonicMillis;
    if (isActiveHeatingCoolingDemand) {
        state_.feedbackEpisodeSubjectSequence = sequence;
        outcome.freshTrustedActiveSequence = sequence;
    }

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

ActuatorPlanTickResult ActuatorPlanner::runPhaseB(
    const ActuatorPlanTickInput& input, const PhaseAOutcome& admission) {
    const std::uint64_t now = input.nowMonotonicMillis;

    // I-1: MalformedCandidate/MalformedSafetyGate oder ein struktureller
    // Tickwert (Safety-Gate-Enum, aktueller Kontext) ist ungueltig.
    if (admission.admissionOutcome ==
            ActuatorAdmissionOutcome::MalformedCandidate ||
        !isKnownSafetyGateStatus(input.safetyGate.status) ||
        !isStructurallyValidContext(input.currentCanonicalContext)) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::InvalidInput,
                            ActuatorPlanReason::MalformedInput);
    }

    // I-2a/I-2b: Parameterklassifikation.
    const ActuatorPlannerParametersValidation parameterValidation =
        classifyActuatorPlannerParameters(parameters_);
    if (parameterValidation ==
        ActuatorPlannerParametersValidation::Unconfigured) {
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
        admission.candidate->demandClass ==
            ActuatorDemandClass::NoValidRequest) {
        return rejectToIdle(admission, now, ActuatorPlanStatus::Idle,
                            ActuatorPlanReason::NoValidRequest);
    }

    // I-7: gehaltener Request-Kontext ist stale zum aktuellen kanonischen
    // Kontext (nur relevant, wenn dieser Tick keinen frischen Kandidaten
    // liefert, der ihn ohnehin ersetzt).
    if (!admission.candidate.has_value() &&
        state_.acceptedCommand.has_value() &&
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
        // Owner-Review ZR2 (Abschnitt 7): der Uebergang der demandClass
        // entscheidet ueber den Akkumulator, unabhaengig davon, ob die alte
        // physische Richtung wegen Minimum-On noch weiterlaeuft. Muss vor
        // dem Ueberschreiben von acceptedCommand ausgewertet werden.
        const ActuatorDemandClass previousDemandClass =
            state_.acceptedCommand.has_value()
                ? state_.acceptedCommand->demandClass
                : ActuatorDemandClass::NoValidRequest;
        const ActuatorDemandClass newDemandClass =
            admission.candidate->demandClass;
        state_.acceptedCommand = admission.candidate;
        if (newDemandClass == ActuatorDemandClass::NeutralOff ||
            newDemandClass == ActuatorDemandClass::AirLimitBlockedOff) {
            // Eine frisch angenommene OFF-Request unterbricht eine laufende
            // Gegenrichtungsbestaetigung auch dann sofort, wenn die alte
            // physische Richtung wegen Minimum-On noch gehalten werden muss.
            state_.counterDirectionCandidate.reset();
            state_.counterDirectionObservedSinceMonotonicMillis = 0U;
            state_.counterDirectionConfirmed = false;
        }
        if (newDemandClass == ActuatorDemandClass::AirLimitBlockedOff) {
            // Uebergang zu AirLimitBlockedOff verwirft betroffenes Guthaben
            // sofort, unbedingt.
            state_.accumulator = PulseAccumulator{};
        } else if (newDemandClass ==
                       ActuatorDemandClass::AirLimitReducedDemand &&
                   previousDemandClass == ActuatorDemandClass::NormalDemand) {
            // Uebergang aus einer weniger restriktiven aktiven Klasse in
            // AirLimitReducedDemand verwirft altes Guthaben vor der naechsten
            // (bereits reduzierten) Gutschrift.
            state_.accumulator = PulseAccumulator{};
        }
    }

    const bool isTeardownRequest =
        !state_.acceptedCommand.has_value() ||
        state_.acceptedCommand->direction == AbstractControlDirection::Idle;
    if (isTeardownRequest) {
        const bool airLimitBlocked =
            state_.acceptedCommand.has_value() &&
            state_.acceptedCommand->demandClass ==
                ActuatorDemandClass::AirLimitBlockedOff;
        const bool physicallyActive =
            physicalDirection() != AbstractControlDirection::Idle;
        const bool withinMinimumOn =
            physicallyActive &&
            state_.currentOnPhaseStartedAtMonotonicMillis.has_value() &&
            !deadlineReached(now,
                             *state_.currentOnPhaseStartedAtMonotonicMillis,
                             parameters_.minimumOnMillis);
        if (withinMinimumOn) {
            // N-1: einzige normale Mindest-On-Halteentscheidung.
            return buildResult(ActuatorPlanStatus::Active,
                               ActuatorPlanReason::MinimumOnTimeHeld,
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

    return evaluateHeatingCoolingDemand(*state_.acceptedCommand,
                                        admission.admissionOutcome, now);
}

ActuatorPlanTickResult ActuatorPlanner::tick(
    const ActuatorPlanTickInput& input) {
    // Abschnitt 6.4: Episodeneintritt wird ausschliesslich ueber diesen Tick
    // erkannt; das Verlassen erfolgt ausschliesslich ueber forceStop().
    if (input.temperatureControlledPhase &&
        !state_.watchdogEpisodeStartedAtMonotonicMillis.has_value()) {
        state_.watchdogEpisodeStartedAtMonotonicMillis =
            input.nowMonotonicMillis;
    }

    const PhaseAOutcome admission = runPhaseA(input);
    ActuatorPlanTickResult result = runPhaseB(input, admission);
    updateFanState(input.nowMonotonicMillis, input.temperatureControlledPhase);
    copyFanStateToResult(result);
    return result;
}

ActuatorPlanTickResult ActuatorPlanner::forceStop(
    std::uint64_t nowMonotonicMillis,
    ActuatorFeedbackEpisodeAtStop feedbackEpisodeAtStop) {
    // Owner-Review ZR5 gilt auch hier: nur ein gehaltenes aktives
    // Heating/Cooling-acceptedCommand ist ein Feedbacksubjekt.
    std::optional<std::uint64_t> trustedSequence;
    if (feedbackEpisodeAtStop ==
            ActuatorFeedbackEpisodeAtStop::ExistingEpisodeOpen &&
        state_.acceptedCommand.has_value() &&
        (state_.acceptedCommand->demandClass ==
             ActuatorDemandClass::NormalDemand ||
         state_.acceptedCommand->demandClass ==
             ActuatorDemandClass::AirLimitReducedDemand)) {
        trustedSequence = state_.acceptedCommand->sequence;
    }

    if (physicalDirection() != AbstractControlDirection::Idle) {
        setPhysicalDirection(AbstractControlDirection::Idle,
                             nowMonotonicMillis);
    }
    clearPlanningState();
    state_.watchdogEpisodeStartedAtMonotonicMillis.reset();
    state_.lastNewRequestAcceptedAtMonotonicMillis.reset();

    if (trustedSequence.has_value()) {
        mergeFeedback(*trustedSequence,
                      PreviousControlRequestFeedback::Disposition::Rejected);
    } else {
        state_.pendingFeedback.reset();
        state_.pendingFeedbackUpdateAvailable = true;
    }
    // Owner-Review F1: a lifecycle stop terminates whatever episode was
    // still open; no subsequent tick may resurrect it.
    state_.feedbackEpisodeSubjectSequence.reset();

    // A lifecycle boundary ends the controlled phase. Preserve an already
    // running inner-fan post-run, while allowing a new controlled episode to
    // re-enable it on its first planner tick.
    if (state_.innerFanActive &&
        !state_.innerFanDeactivationRequestedAtMonotonicMillis.has_value()) {
        state_.innerFanDeactivationRequestedAtMonotonicMillis =
            nowMonotonicMillis;
    }
    updateFanState(nowMonotonicMillis, false);

    return buildResult(ActuatorPlanStatus::Idle,
                       ActuatorPlanReason::NeutralIdle,
                       ActuatorAdmissionOutcome::NoCandidate);
}

void ActuatorPlanner::closeFeedbackEpisodeForOutstandingEvaluation() {
    // Owner-Review F1: a newly registered outstandingEvaluation always closes
    // the previous subject, even if no disposition had been recorded for it
    // yet (e.g. an accepted active candidate observed on an otherwise
    // rejected tick).
    state_.feedbackEpisodeSubjectSequence.reset();
    if (state_.pendingFeedback.has_value()) {
        state_.pendingFeedback.reset();
        state_.pendingFeedbackUpdateAvailable = true;
    }
}

PendingControlRequestFeedbackUpdate ActuatorPlanner::takeFeedbackUpdate() {
    if (!state_.pendingFeedbackUpdateAvailable) return {};

    PendingControlRequestFeedbackUpdate update;
    update.changed = true;
    if (state_.pendingFeedback.has_value()) {
        update.feedback =
            PreviousControlRequestFeedback{state_.pendingFeedback->sequence,
                                           state_.pendingFeedback->disposition};
    }
    state_.pendingFeedbackUpdateAvailable = false;
    return update;
}

void ActuatorPlanner::applyExternalWatchdogFaultReset(
    std::uint64_t /*nowMonotonicMillis*/) {
    state_.latchedWatchdogFault.reset();
}

}  // namespace fermentation
