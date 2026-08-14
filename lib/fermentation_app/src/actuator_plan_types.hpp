#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

#include "control_context_types.hpp"
#include "sensor_selection_types.hpp"
#include "temperature_control_types.hpp"

namespace fermentation {

// #24 (oder Interims-Composition-Root) liefert dieses Urteil pro Tick. Bis
// #24 real verdrahtet ist, bleibt der Composition-Root-Default Unresolved;
// dies fuehrt strukturell zu Idle und ist kein implizites "Safety=erlaubt".
enum class ActuatorSafetyGateStatus : std::uint8_t {
    Unresolved,
    Allowed,
    ImmediateStop,
};

struct ActuatorSafetyGateInput {
    ActuatorSafetyGateStatus status{ActuatorSafetyGateStatus::Unresolved};
};

// Reine, fuer #24 konsumierbare Evidenz. Freigabe ausschliesslich ueber
// ActuatorPlanner::applyExternalWatchdogFaultReset().
struct ActuatorWatchdogFaultEvidence {
    std::uint64_t detectedAtMonotonicMillis{0U};
    std::uint64_t lastObservedSequenceHighWatermarkBeforeFault{0U};
};

// Reine, tabellarische Abbildung der bereits abschliessenden #22-Status-/
// Reason-/AirLimitState-Matrix (issue-22-pi-control-air-limits-plan.md,
// Abschnitt 7.2). Kein neuer Schwellenwert, keine Neuberechnung.
enum class ActuatorDemandClass : std::uint8_t {
    NoValidRequest,         // Unavailable oder InvalidInput - keine ControlRequest
    NeutralOff,              // Off / NeutralBand
    AirLimitBlockedOff,      // Off / AirLimitBlocked
    AirLimitReducedDemand,   // Demand / AirLimitReduced
    NormalDemand,            // Demand / None oder Saturated
};

[[nodiscard]] inline ActuatorDemandClass classifyActuatorDemand(
    const TemperatureControlResult& controlResult) {
    switch (controlResult.status) {
        case TemperatureControlStatus::Unavailable:
        case TemperatureControlStatus::InvalidInput:
            return ActuatorDemandClass::NoValidRequest;
        case TemperatureControlStatus::Off:
            return controlResult.reason ==
                           TemperatureControlReason::AirLimitBlocked
                       ? ActuatorDemandClass::AirLimitBlockedOff
                       : ActuatorDemandClass::NeutralOff;
        case TemperatureControlStatus::Demand:
            return controlResult.reason ==
                           TemperatureControlReason::AirLimitReduced
                       ? ActuatorDemandClass::AirLimitReducedDemand
                       : ActuatorDemandClass::NormalDemand;
    }
    return ActuatorDemandClass::NoValidRequest;
}

enum class ActuatorPlanStatus : std::uint8_t {
    Active,
    Idle,
    Unconfigured,
    InvalidInput,
};

enum class ActuatorPlanReason : std::uint8_t {
    MalformedInput,
    NoCommissioning,
    InvalidConfiguration,
    TimeInvalid,
    SafetyGateUnresolved,
    ExternalSafetyOverride,
    RequestWatchdogFaultLatched,
    StaleRequestWatchdog,
    NoValidRequest,
    StaleRequestContext,
    MinimumOnTimeHeld,
    MinimumOffTimeHeld,
    PolarityDeadTimeHeld,
    DirectionChangeApplied,
    NeutralIdle,
    AirLimitBlocked,
    AccumulatingBelowThreshold,
    MinimumPulseTriggered,
    CounterDirectionConfirming,
    WindowPulseMissed,
    ScheduledWithinWindow,
};

// Es existiert zu jedem Zeitpunkt genau EIN PulseAccumulator. direction ==
// Idle bedeutet "leer/nicht gebunden"; nur Heating oder Cooling tragen
// jemals ein Guthaben > 0.
struct PulseAccumulator {
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    double accumulatedMillis{0.0};
};

// Getrennt von der physischen reason-Entscheidung und vom internen
// Feedback-Handoff: beantwortet ausschliesslich "was geschah mit dem neuen
// Kandidaten?".
enum class ActuatorAdmissionOutcome : std::uint8_t {
    NoCandidate,
    Accepted,
    MalformedCandidate,
    MalformedSafetyGate,
    DuplicateOrOldSequence,
    StaleOnArrivalWatchdog,
    StaleOnArrivalContext,
};

struct ActuatorPlannerParameters {
    std::uint64_t switchingWindowMillis{0U};
    std::uint64_t minimumOnMillis{0U};
    std::uint64_t minimumOffMillis{0U};
    std::uint64_t polarityDeadTimeMillis{0U};
    std::uint64_t pulseAccumulatorCapMillis{0U};
    double counterDirectionConfirmationQuoteThreshold{0.0};
    std::uint64_t counterDirectionConfirmationDurationMillis{0U};
    std::uint64_t requestWatchdogMillis{0U};
    std::uint64_t outerFanPostRunMillis{0U};
    std::uint64_t innerFanPostRunMillis{0U};
};

enum class ActuatorPlannerParametersValidation : std::uint8_t {
    Unconfigured,
    Valid,
    Invalid,
};

[[nodiscard]] inline ActuatorPlannerParametersValidation
classifyActuatorPlannerParameters(const ActuatorPlannerParameters& p) {
    const bool allZero =
        p.switchingWindowMillis == 0U && p.minimumOnMillis == 0U &&
        p.minimumOffMillis == 0U && p.polarityDeadTimeMillis == 0U &&
        p.pulseAccumulatorCapMillis == 0U &&
        p.counterDirectionConfirmationQuoteThreshold == 0.0 &&
        p.counterDirectionConfirmationDurationMillis == 0U &&
        p.requestWatchdogMillis == 0U && p.outerFanPostRunMillis == 0U &&
        p.innerFanPostRunMillis == 0U;
    if (allZero) {
        return ActuatorPlannerParametersValidation::Unconfigured;
    }

    // 2^53, groesste im IEEE-754-double exakt darstellbare Ganzzahl; schliesst
    // Praezisions-/Rundungsfehler in der Fensterarithmetik (Abschnitt 8.1)
    // sowie einen Additionsueberlauf strukturell aus. innerFanPostRunMillis
    // erhaelt bewusst keine Relation: 0 ist ein zulaessiger "kein
    // Nachlauf"-Wert (ACTUATOR_TIMING.md, Innenluefter ist "konfigurierbar",
    // nicht "zwingend").
    constexpr std::uint64_t kMaxExactDoubleInteger = 9007199254740992ULL;

    const bool valid =
        p.switchingWindowMillis > 0U && p.minimumOnMillis > 0U &&
        p.minimumOnMillis <= p.switchingWindowMillis &&
        p.minimumOffMillis > 0U && p.polarityDeadTimeMillis > 0U &&
        p.pulseAccumulatorCapMillis >= p.minimumOnMillis &&
        std::isfinite(p.counterDirectionConfirmationQuoteThreshold) &&
        p.counterDirectionConfirmationQuoteThreshold > 0.0 &&
        p.counterDirectionConfirmationQuoteThreshold <= 1.0 &&
        p.counterDirectionConfirmationDurationMillis > 0U &&
        p.requestWatchdogMillis > 0U && p.outerFanPostRunMillis > 0U &&
        p.switchingWindowMillis <= kMaxExactDoubleInteger;

    return valid ? ActuatorPlannerParametersValidation::Valid
                 : ActuatorPlannerParametersValidation::Invalid;
}

struct AcceptedControlCommand {
    std::uint64_t sequence{0U};
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    double timeQuote{0.0};
    ActuatorDemandClass demandClass{ActuatorDemandClass::NoValidRequest};
    ControlRequestContext contextAtAcceptance;
};

// Am Fensterstart genau einmal erzeugter, unveraenderlicher Planungssnapshot.
// Er beschreibt nur die natuerliche Pulsplatzierung dieses Fensters; er
// behauptet weder eine physische Freigabe noch einen laufenden Nachlauf.
struct ActiveSwitchingWindow {
    std::uint64_t startMonotonicMillis{0U};
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    // Immutable physical ownership of this natural window. This snapshot is
    // diagnostic/physical evidence only; it is not the prerequisite for the
    // anti-windup disposition of a later same-direction request.
    std::uint64_t sourceRequestSequence{0U};
    std::uint64_t scheduledOnMillis{0U};
    // Exactly one first-start attempt is allowed for this natural window.
    // A late tick that cannot guarantee minimumOnMillis sets this marker and
    // discards the entire pulse without retry or carry into the next window.
    bool pulseStartAttempted{false};
    // Diagnostic-only: true iff scheduledOnMillis was produced by the pulse
    // accumulator crossing minimumOnMillis (Abschnitt 8.1), false when it
    // came directly from a quote already >= minimumOnMillis. Not part of
    // the physical contract; distinguishes ActuatorPlanReason::
    // MinimumPulseTriggered from ScheduledWithinWindow (Owner-Review ZR6).
    bool minimumPulseFromAccumulator{false};
};

// Getrenntes Feedback-Handoff: Das Subjekt, fuer das #23 als naechstes ein
// PreviousControlRequestFeedback an #22 liefert, ist NICHT zwangslaeufig
// identisch mit dem aktuell physisch massgeblichen acceptedCommand.
struct PendingControlRequestFeedback {
    std::uint64_t sequence{0U};
    PreviousControlRequestFeedback::Disposition disposition{
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
};

struct ActuatorPlannerRuntimeState {
    std::optional<AcceptedControlCommand> acceptedCommand;
    std::optional<ActiveSwitchingWindow> activeWindow;
    PulseAccumulator accumulator;

    // Tatsaechlicher physischer Ausgang, getrennt vom Planungs-Snapshot. Ein
    // Window-Off-Anteil setzt diesen Wert auf Idle, ohne activeWindow
    // zwingend zu loeschen.
    AbstractControlDirection lastAppliedDirection{
        AbstractControlDirection::Idle};

    // Beginn der aktuellen ununterbrochenen physischen Einschaltphase;
    // nullopt, sobald lastAppliedDirection Idle ist. Alleinige Zeitbasis fuer
    // die normale Mindest-Einschaltzeit.
    std::optional<std::uint64_t> currentOnPhaseStartedAtMonotonicMillis;

    // Letzte physisch deaktivierte Richtung und ihr Zeitpunkt. Diese Felder
    // werden bei JEDEM tatsaechlichen Active -> Idle gesetzt, auch beim
    // normalen Window-Off-Anteil, bei forceStop() und bei Fail-closed. Sie
    // werden nie geloescht und sind die alleinige Mindest-Auszeit-/
    // Totzeit-Zeitbasis.
    std::optional<AbstractControlDirection> lastPhysicalDeactivationDirection;
    std::optional<std::uint64_t> lastPhysicalDeactivationAtMonotonicMillis;

    std::optional<AbstractControlDirection> counterDirectionCandidate;
    std::uint64_t counterDirectionObservedSinceMonotonicMillis{0U};
    bool counterDirectionConfirmed{false};

    // Replay high-watermark: every new structurally valid sequence advances
    // this field before stale-on-arrival admission is decided.
    std::optional<std::uint64_t> lastObservedSequenceHighWatermark;
    // H heartbeat, valid only while a watched temperature-control episode is
    // active. It is rebased at episode entry and cleared on episode exit.
    std::optional<std::uint64_t> lastNewRequestAcceptedAtMonotonicMillis;
    std::optional<std::uint64_t> watchdogEpisodeStartedAtMonotonicMillis;
    std::optional<ActuatorWatchdogFaultEvidence> latchedWatchdogFault;

    // Last actually observed disposition for the currently tracked feedback
    // episode. A new active sequence starts with no disposition yet; Phase B
    // records the first real outcome. Once recorded, severity only increases
    // until a new sequence or an explicit OFF/no-request closes the window.
    std::optional<PendingControlRequestFeedback> pendingFeedback;
    bool pendingFeedbackUpdateAvailable{false};

    bool outerFanActive{false};
    std::optional<std::uint64_t> outerFanDeactivationRequestedAtMonotonicMillis;
    bool innerFanActive{false};
    std::optional<std::uint64_t> innerFanDeactivationRequestedAtMonotonicMillis;
};

struct ActuatorPlanTickInput {
    std::uint64_t nowMonotonicMillis{0U};
    std::optional<TemperatureControlResult> newEvaluation;
    ControlRequestContext currentCanonicalContext;
    bool temperatureControlledPhase{false};
    ActuatorSafetyGateInput safetyGate;
};

struct ActuatorPlanTickResult {
    ActuatorPlanStatus status{ActuatorPlanStatus::Unconfigured};
    ActuatorPlanReason reason{ActuatorPlanReason::NoCommissioning};
    AbstractControlDirection appliedDirection{AbstractControlDirection::Idle};
    bool outerFanEnabled{false};
    bool innerFanEnabled{false};
    bool counterDirectionConfirming{false};
    ActuatorAdmissionOutcome admissionOutcome{
        ActuatorAdmissionOutcome::NoCandidate};
    std::optional<std::uint64_t> acceptedCommandSequence;
    bool watchdogFaultActive{false};
};

}  // namespace fermentation
