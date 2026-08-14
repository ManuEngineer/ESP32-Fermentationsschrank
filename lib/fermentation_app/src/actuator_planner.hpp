#pragma once

#include <cstdint>
#include <optional>

#include "actuator_plan_types.hpp"

namespace fermentation {

// Signals to forceStop() whether a feedback episode for the currently held
// acceptedCommand is still open, or was already closed earlier in the same
// orchestrator step by a newer outstandingEvaluation (Abschnitt 8.2,
// Trusted-Sequence-Regel; Abschnitt 11).
enum class ActuatorFeedbackEpisodeAtStop : std::uint8_t {
    ExistingEpisodeOpen,
    ClosedByOutstandingEvaluation,
};

// Reine, deterministische Aktorplanung: uebersetzt eine gueltige abstrakte
// #22-ControlRequest in zeitlich korrekte, abstrakte Aktorbefehle. Kein
// Sink-, kein GPIO-Zugriff (ADR-013).
class ActuatorPlanner {
   public:
    explicit ActuatorPlanner(ActuatorPlannerParameters parameters);

    [[nodiscard]] ActuatorPlanTickResult tick(
        const ActuatorPlanTickInput& input);
    // Die einzige RAM-Stop-Operation fuer alle Lifecycle-Grenzen (Abschnitt
    // 11). Nimmt keinen Caller-uebergebenen Sequence-Wert entgegen.
    [[nodiscard]] ActuatorPlanTickResult forceStop(
        std::uint64_t nowMonotonicMillis,
        ActuatorFeedbackEpisodeAtStop feedbackEpisodeAtStop);
    // Closes the feedback subject consumed by a new #22 evaluation while
    // retaining physical planning state until the evaluation is observed.
    void closeFeedbackEpisodeForOutstandingEvaluation();
    // Single-use application handoff. An unchanged disposition is never
    // replayed to the Application boundary.
    [[nodiscard]] PendingControlRequestFeedbackUpdate takeFeedbackUpdate();
    // Ausschliesslich von #24-getriebener Logik aufgerufen (Abschnitt 8.6);
    // #23 ruft dies an keiner Stelle selbst auf.
    void applyExternalWatchdogFaultReset(std::uint64_t nowMonotonicMillis);

    [[nodiscard]] const ActuatorPlannerRuntimeState& state() const;
    [[nodiscard]] const ActuatorPlannerParameters& parameters() const;

   private:
    // Buendelt, was Phase A (Abschnitt 6.2) fuer Phase B (Abschnitt 8.2)
    // vormerkt: das Ergebnis der Annahmeentscheidung sowie ein Kandidat fuer
    // eine physische Neubewertung (nur bei Accepted/MalformedSafetyGate).
    // Owner-Review R1: die frisch vertrauenswuerdig gewordene
    // Feedback-Sequence wird NICHT mehr hier zwischengespeichert, sondern
    // ausschliesslich in state_.feedbackEpisodeSubjectSequence gefuehrt -
    // der alleinigen Autoritaet fuer das aktuell offene Feedbacksubjekt,
    // gelesen von rejectToIdle()/forceStop() ueber beliebig viele Ticks
    // hinweg, nicht nur innerhalb des Ticks, der sie eroeffnet hat.
    struct PhaseAOutcome {
        ActuatorAdmissionOutcome admissionOutcome{
            ActuatorAdmissionOutcome::NoCandidate};
        std::optional<AcceptedControlCommand> candidate;
    };

    [[nodiscard]] PhaseAOutcome runPhaseA(const ActuatorPlanTickInput& input);
    [[nodiscard]] ActuatorPlanTickResult runPhaseB(
        const ActuatorPlanTickInput& input, const PhaseAOutcome& admission);

    [[nodiscard]] bool runningWatchdogTripped(
        const ActuatorPlanTickInput& input) const;
    [[nodiscard]] bool hasRetrogradeTimeReference(std::uint64_t now) const;

    // Zentrale physische Uebergangsfunktion (Abschnitt 8.1/8.2-Praeambel).
    // Nur von Idle aus in eine Richtung und umgekehrt; ein direkter
    // Richtungswechsel ist ein Aufruferfehler und wird hier nicht
    // stillschweigend abgefangen.
    void setPhysicalDirection(AbstractControlDirection next, std::uint64_t now);
    // Verwirft acceptedCommand, activeWindow, accumulator und den
    // Gegenrichtungskandidaten (Abschnitt 8.4).
    void clearPlanningState();
    void mergeFeedback(std::uint64_t sequence,
                       PreviousControlRequestFeedback::Disposition disposition);
    // Owner-Review F1: only mutates pendingFeedback when command.sequence is
    // the currently open feedback episode's subject; a stale acceptedCommand
    // still governed physically (minimum-on, teardown) after its own
    // episode closed is silently skipped.
    void mergeFeedbackForDemand(const AcceptedControlCommand& command,
                                ActuatorPlanReason reason);
    // Atomically closes the previous feedback episode's subject and opens
    // the given one (or none), resetting pendingFeedback (Abschnitt 6.2).
    void openFeedbackEpisode(std::optional<std::uint64_t> subjectSequence);
    void updateFanState(std::uint64_t now, bool temperatureControlledPhase);
    void copyFanStateToResult(ActuatorPlanTickResult& result) const;
    [[nodiscard]] ActuatorPlanTickResult buildResult(
        ActuatorPlanStatus status, ActuatorPlanReason reason,
        ActuatorAdmissionOutcome admissionOutcome) const;
    // Gemeinsamer Klasse-I-/N-3-Pfad: sofortige physische Idle-Abschaltung,
    // vollstaendige Bereinigung und Trusted-Sequence-Feedback.
    [[nodiscard]] ActuatorPlanTickResult rejectToIdle(
        const PhaseAOutcome& admission, std::uint64_t now,
        ActuatorPlanStatus status, ActuatorPlanReason reason);

    // Arming-Regel (Abschnitt 8.1): physische Freigabefaehigkeit einer
    // Richtung am gegebenen Zeitpunkt gegen den physischen
    // Deaktivierungsanker.
    [[nodiscard]] bool armingAllowed(AbstractControlDirection direction,
                                     std::uint64_t atMillis) const;
    // Akkumulator (Abschnitt 8.4): Gutschrift der unrundeten Quote, Cap
    // durch pulseAccumulatorCapMillis.
    void creditAccumulator(AbstractControlDirection direction,
                           double quoteMillis);
    // Fensterstart-Ereignis (Abschnitt 8.1): Erststart, gleichgerichteter
    // Neustart, regulaerer Folgefensterbeginn oder bestaetigte
    // B-Neuanlage - ein einziger Erzeugungspfad fuer alle vier Faelle.
    void startFreshWindow(const AcceptedControlCommand& source,
                          std::uint64_t startMonotonicMillis);

    struct WindowPhysicalTick {
        bool active{false};
        ActuatorPlanReason reason{ActuatorPlanReason::NeutralIdle};
    };
    // Wendet die Arming-/Mindest-Einschaltzeit-Aktivierungsgates (8.1 Punkt
    // 1-4) auf das aktuelle activeWindow an und liefert den natuerlichen
    // physischen Wunschzustand dieses Ticks samt Reason.
    [[nodiscard]] WindowPhysicalTick applyWindowPhysicalTick(std::uint64_t now);

    // Heating-/Cooling-Zweig der Klasse N (Abschnitt 8.1, 8.4, 8.5): Fenster-
    // fortschritt, Gegenrichtungsbestaetigung und physische Umsetzung.
    [[nodiscard]] ActuatorPlanTickResult evaluateHeatingCoolingDemand(
        const AcceptedControlCommand& command,
        ActuatorAdmissionOutcome admissionOutcome, std::uint64_t now);

    [[nodiscard]] AbstractControlDirection plannedDirection() const;
    [[nodiscard]] AbstractControlDirection physicalDirection() const;

    ActuatorPlannerParameters parameters_;
    ActuatorPlannerRuntimeState state_;
};

}  // namespace fermentation
