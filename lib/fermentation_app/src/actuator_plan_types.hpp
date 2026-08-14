#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

#include "control_context_types.hpp"
#include "fault_types.hpp"
#include "sensor_selection_types.hpp"
#include "temperature_control_types.hpp"

namespace fermentation {

class ActuatorPlanner;
class SafetyFaultService;

// #24 (oder Interims-Composition-Root) liefert dieses Urteil pro Tick. Bis
// #24 real verdrahtet ist, bleibt der Composition-Root-Default Unresolved;
// dies fuehrt strukturell zu Idle und ist kein implizites "Safety=erlaubt".
enum class ActuatorSafetyGateStatus : std::uint8_t {
    Unresolved,
    Allowed,
    ImmediateStop,
    SafetyRecovery,
    SAFETY_RECOVERY = SafetyRecovery,
};

// Qualification is an observation, not a safety authority.  It contains
// typed producer results; the #24 core derives the positive capability from
// it after checking the current FaultCore and SafetyStateRecord.
enum class SafetyRecoveryCheck : std::uint8_t {
    Unknown,
    Passed,
    Failed,
};

struct SafetyRecoveryQualification {
    FaultInstanceId targetFault;
    AbstractControlDirection triggeringDirection{
        AbstractControlDirection::Unknown};
    AbstractControlDirection recoveryDirection{
        AbstractControlDirection::Unknown};
    std::uint32_t faultRevision{0U};
    std::uint32_t safetyEvidenceRevision{0U};
    std::uint8_t attemptIndex{0U};
    std::uint8_t maxAttempts{0U};
    std::uint64_t sequence{0U};
    std::uint64_t createdAtMonotonicMillis{0U};
    double timeQuote{0.0};
    ControlRequestContext contextAtQualification;
    std::uint32_t qualifiedSensorEvidence{0U};
    std::uint32_t qualifiedFanEvidence{0U};
    std::uint32_t qualifiedActuatorEvidence{0U};
    SafetyRecoveryCheck hardLimit{SafetyRecoveryCheck::Unknown};
    SafetyRecoveryCheck sensorConflict{SafetyRecoveryCheck::Unknown};
    SafetyRecoveryCheck triggeringDirectionOff{SafetyRecoveryCheck::Unknown};
    SafetyRecoveryCheck safeCurrentWhenAvailable{SafetyRecoveryCheck::Unknown};
    SafetyRecoveryCheck minimumOffTimeElapsed{SafetyRecoveryCheck::Unknown};
    SafetyRecoveryCheck polarityDeadTimeElapsed{SafetyRecoveryCheck::Unknown};
    std::uint32_t safetyRecoveryParametersRevision{0U};
};

// Bounded S3-004 capability.  There is deliberately no public constructor or
// public mutable field.  Only SafetyFaultService can issue one; the planner
// may inspect one but cannot mint a replacement.  A copied capability remains
// stale-checkable through the issuer marker and all current revisions.
class SafetyRecoveryRequest final {
   public:
    [[nodiscard]] FaultInstanceId targetFault() const { return targetFault_; }
    [[nodiscard]] AbstractControlDirection triggeringDirection() const {
        return triggeringDirection_;
    }
    [[nodiscard]] AbstractControlDirection recoveryDirection() const {
        return recoveryDirection_;
    }
    [[nodiscard]] std::uint32_t faultRevision() const { return faultRevision_; }
    [[nodiscard]] std::uint32_t safetyEvidenceRevision() const {
        return safetyEvidenceRevision_;
    }
    [[nodiscard]] std::uint8_t attemptIndex() const { return attemptIndex_; }
    [[nodiscard]] std::uint8_t maxAttempts() const { return maxAttempts_; }
    [[nodiscard]] std::uint64_t sequence() const { return sequence_; }
    [[nodiscard]] std::uint64_t createdAtMonotonicMillis() const {
        return createdAtMonotonicMillis_;
    }
    [[nodiscard]] double timeQuote() const { return timeQuote_; }
    [[nodiscard]] const ControlRequestContext& contextAtQualification() const {
        return contextAtQualification_;
    }
    [[nodiscard]] std::uint32_t qualifiedSensorEvidence() const {
        return qualifiedSensorEvidence_;
    }
    [[nodiscard]] std::uint32_t qualifiedFanEvidence() const {
        return qualifiedFanEvidence_;
    }
    [[nodiscard]] std::uint32_t qualifiedActuatorEvidence() const {
        return qualifiedActuatorEvidence_;
    }
    [[nodiscard]] bool hardLimitNotReached() const {
        return hardLimitNotReached_;
    }
    [[nodiscard]] bool noSensorConflict() const { return noSensorConflict_; }
    [[nodiscard]] bool triggeringDirectionOff() const {
        return triggeringDirectionOff_;
    }
    [[nodiscard]] bool safeCurrentWhenAvailable() const {
        return safeCurrentWhenAvailable_;
    }
    [[nodiscard]] bool minimumOffTimeElapsed() const {
        return minimumOffTimeElapsed_;
    }
    [[nodiscard]] bool polarityDeadTimeElapsed() const {
        return polarityDeadTimeElapsed_;
    }
    [[nodiscard]] std::uint32_t safetyRecoveryParametersRevision() const {
        return safetyRecoveryParametersRevision_;
    }
    [[nodiscard]] bool structurallyValid() const;
    [[nodiscard]] bool issuedBy(const SafetyFaultService* issuer) const {
        return issuer != nullptr && issuer_ == issuer;
    }

   private:
    friend class SafetyFaultService;
    friend class ActuatorPlanner;

    SafetyRecoveryRequest(const SafetyRecoveryQualification& qualification,
                          const SafetyFaultService* issuer)
        : targetFault_(qualification.targetFault),
          triggeringDirection_(qualification.triggeringDirection),
          recoveryDirection_(qualification.recoveryDirection),
          faultRevision_(qualification.faultRevision),
          safetyEvidenceRevision_(qualification.safetyEvidenceRevision),
          attemptIndex_(qualification.attemptIndex),
          maxAttempts_(qualification.maxAttempts),
          sequence_(qualification.sequence),
          createdAtMonotonicMillis_(qualification.createdAtMonotonicMillis),
          timeQuote_(qualification.timeQuote),
          contextAtQualification_(qualification.contextAtQualification),
          qualifiedSensorEvidence_(qualification.qualifiedSensorEvidence),
          qualifiedFanEvidence_(qualification.qualifiedFanEvidence),
          qualifiedActuatorEvidence_(qualification.qualifiedActuatorEvidence),
          hardLimitNotReached_(qualification.hardLimit ==
                               SafetyRecoveryCheck::Passed),
          noSensorConflict_(qualification.sensorConflict ==
                            SafetyRecoveryCheck::Passed),
          triggeringDirectionOff_(qualification.triggeringDirectionOff ==
                                  SafetyRecoveryCheck::Passed),
          safeCurrentWhenAvailable_(qualification.safeCurrentWhenAvailable ==
                                    SafetyRecoveryCheck::Passed),
          minimumOffTimeElapsed_(qualification.minimumOffTimeElapsed ==
                                 SafetyRecoveryCheck::Passed),
          polarityDeadTimeElapsed_(qualification.polarityDeadTimeElapsed ==
                                   SafetyRecoveryCheck::Passed),
          safetyRecoveryParametersRevision_(
              qualification.safetyRecoveryParametersRevision),
          issuer_(issuer) {}

    FaultInstanceId targetFault_;
    AbstractControlDirection triggeringDirection_{
        AbstractControlDirection::Unknown};
    AbstractControlDirection recoveryDirection_{
        AbstractControlDirection::Unknown};
    std::uint32_t faultRevision_{0U};
    std::uint32_t safetyEvidenceRevision_{0U};
    std::uint8_t attemptIndex_{0U};
    std::uint8_t maxAttempts_{0U};
    std::uint64_t sequence_{0U};
    std::uint64_t createdAtMonotonicMillis_{0U};
    double timeQuote_{0.0};
    ControlRequestContext contextAtQualification_;
    std::uint32_t qualifiedSensorEvidence_{0U};
    std::uint32_t qualifiedFanEvidence_{0U};
    std::uint32_t qualifiedActuatorEvidence_{0U};
    bool hardLimitNotReached_{false};
    bool noSensorConflict_{false};
    bool triggeringDirectionOff_{false};
    bool safeCurrentWhenAvailable_{false};
    bool minimumOffTimeElapsed_{false};
    bool polarityDeadTimeElapsed_{false};
    std::uint32_t safetyRecoveryParametersRevision_{0U};
    const SafetyFaultService* issuer_{nullptr};
};

inline bool SafetyRecoveryRequest::structurallyValid() const {
    return issuer_ != nullptr && targetFault_.valid() &&
           triggeringDirection_ != AbstractControlDirection::Unknown &&
           recoveryDirection_ != AbstractControlDirection::Unknown &&
           triggeringDirection_ != recoveryDirection_ && faultRevision_ != 0U &&
           safetyEvidenceRevision_ != 0U && attemptIndex_ >= 1U &&
           maxAttempts_ >= attemptIndex_ && maxAttempts_ <= 2U &&
           sequence_ != 0U && std::isfinite(timeQuote_) && timeQuote_ > 0.0 &&
           timeQuote_ <= 1.0 && qualifiedSensorEvidence_ != 0U &&
           qualifiedFanEvidence_ != 0U && qualifiedActuatorEvidence_ != 0U &&
           hardLimitNotReached_ && noSensorConflict_ &&
           triggeringDirectionOff_ && safeCurrentWhenAvailable_ &&
           minimumOffTimeElapsed_ && polarityDeadTimeElapsed_ &&
           safetyRecoveryParametersRevision_ != 0U;
}

struct ActuatorSafetyGateInput {
    ActuatorSafetyGateStatus status{ActuatorSafetyGateStatus::Unresolved};
    std::optional<SafetyRecoveryRequest> safetyRecovery;

    ActuatorSafetyGateInput() = default;
    explicit ActuatorSafetyGateInput(ActuatorSafetyGateStatus value)
        : status(value) {}

    [[nodiscard]] bool hasRecoveryAuthority() const {
        return status == ActuatorSafetyGateStatus::SafetyRecovery &&
               safetyRecovery.has_value() &&
               safetyRecovery->issuedBy(authority_);
    }

   private:
    friend class SafetyFaultService;
    const SafetyFaultService* authority_{nullptr};
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
    NoValidRequest,      // Unavailable oder InvalidInput - keine ControlRequest
    NeutralOff,          // Off / NeutralBand
    AirLimitBlockedOff,  // Off / AirLimitBlocked
    AirLimitReducedDemand,  // Demand / AirLimitReduced
    NormalDemand,           // Demand / None oder Saturated
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
    SafetyRecovery,
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
    // Diagnostic-only: true once this window's single first-activation
    // attempt actually turned the physical output on. Distinguishes a
    // pulse's own normal, scheduled off-portion (ScheduledWithinWindow)
    // from a pulse that never successfully started at all
    // (WindowPulseMissed / the applicable arming-gate reason). Not part of
    // the physical contract (Owner-Review RZ4).
    bool pulseStartedSuccessfully{false};
};

// Getrenntes Feedback-Handoff: Das Subjekt, fuer das #23 als naechstes ein
// PreviousControlRequestFeedback an #22 liefert, ist NICHT zwangslaeufig
// identisch mit dem aktuell physisch massgeblichen acceptedCommand.
struct PendingControlRequestFeedback {
    std::uint64_t sequence{0U};
    PreviousControlRequestFeedback::Disposition disposition{
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
};

struct PendingControlRequestFeedbackUpdate {
    bool changed{false};
    std::optional<PreviousControlRequestFeedback> feedback;
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

    // Owner-Review F1: the sequence, if any, that currently owns the open
    // feedback episode (Abschnitt 9.1). Distinct from acceptedCommand, which
    // may keep governing an old physical direction (minimum-on, teardown)
    // after its episode already closed. Only a mergeFeedbackForDemand() call
    // whose command matches this sequence may still mutate pendingFeedback;
    // a stale acceptedCommand's continued physical governance must not
    // resurrect an already-closed or foreign subject. Set exactly where
    // Phase A opens/closes a feedback window (Abschnitt 6.2); left
    // unchanged for a nullopt tick or a DuplicateOrOldSequence outcome.
    std::optional<std::uint64_t> feedbackEpisodeSubjectSequence;

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
