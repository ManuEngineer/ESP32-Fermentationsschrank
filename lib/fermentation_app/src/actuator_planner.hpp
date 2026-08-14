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

    [[nodiscard]] ActuatorPlanTickResult tick(const ActuatorPlanTickInput& input);
    // Die einzige RAM-Stop-Operation fuer alle Lifecycle-Grenzen (Abschnitt
    // 11). Nimmt keinen Caller-uebergebenen Sequence-Wert entgegen.
    [[nodiscard]] ActuatorPlanTickResult forceStop(
        std::uint64_t nowMonotonicMillis,
        ActuatorFeedbackEpisodeAtStop feedbackEpisodeAtStop);
    // Ausschliesslich von #24-getriebener Logik aufgerufen (Abschnitt 8.6);
    // #23 ruft dies an keiner Stelle selbst auf.
    void applyExternalWatchdogFaultReset(std::uint64_t nowMonotonicMillis);

    [[nodiscard]] const ActuatorPlannerRuntimeState& state() const;
    [[nodiscard]] const ActuatorPlannerParameters& parameters() const;

   private:
    // Buendelt, was Phase A (Abschnitt 6.2) fuer Phase B (Abschnitt 8.2)
    // vormerkt: das Ergebnis der Annahmeentscheidung, ein Kandidat fuer eine
    // physische Neubewertung (nur bei Accepted/MalformedSafetyGate) sowie -
    // ausschliesslich fuer eine aktive Heating/Cooling-ControlRequest
    // (NormalDemand/AirLimitReducedDemand) - deren in diesem Tick frisch
    // vertrauenswuerdig gewordene Sequence (Accepted, MalformedSafetyGate,
    // StaleOnArrivalWatchdog, StaleOnArrivalContext - siehe Abschnitt 6.2
    // Schritt 6 und die Trusted-Sequence-Regel in 8.2). Eine gueltige OFF-
    // Request (NeutralOff/AirLimitBlockedOff) setzt dieses Feld NIE: #22
    // oeffnet ein Feedbackfenster ausschliesslich fuer eine aktive
    // Heating/Cooling-Request (Owner-Review ZR5/RZ6).
    struct PhaseAOutcome {
        ActuatorAdmissionOutcome admissionOutcome{
            ActuatorAdmissionOutcome::NoCandidate};
        std::optional<AcceptedControlCommand> candidate;
        std::optional<std::uint64_t> freshTrustedActiveSequence;
        bool episodeClosedThisTick{false};
    };

    [[nodiscard]] PhaseAOutcome runPhaseA(const ActuatorPlanTickInput& input);
    [[nodiscard]] ActuatorPlanTickResult runPhaseB(
        const ActuatorPlanTickInput& input, const PhaseAOutcome& admission);

    [[nodiscard]] bool runningWatchdogTripped(
        const ActuatorPlanTickInput& input) const;
    [[nodiscard]] bool hasRetrogradeTimeReference(std::uint64_t now) const;
    [[nodiscard]] std::optional<std::uint64_t> resolveTrustedSequenceForRejection(
        const PhaseAOutcome& admission) const;

    // Zentrale physische Uebergangsfunktion (Abschnitt 8.1/8.2-Praeambel).
    // Nur von Idle aus in eine Richtung und umgekehrt; ein direkter
    // Richtungswechsel ist ein Aufruferfehler und wird hier nicht
    // stillschweigend abgefangen.
    void setPhysicalDirection(AbstractControlDirection next, std::uint64_t now);
    // Verwirft acceptedCommand, activeWindow, accumulator und den
    // Gegenrichtungskandidaten (Abschnitt 8.4).
    void clearPlanningState();
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
