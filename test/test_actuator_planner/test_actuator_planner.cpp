#include <limits>

#include <unity.h>

#include "actuator_planner.hpp"

namespace {

using namespace fermentation;

// Plausible, frei gewaehlte Testwerte (keine Produktionskommissionierung).
ActuatorPlannerParameters testParameters() {
    ActuatorPlannerParameters parameters;
    parameters.switchingWindowMillis = 10'000U;
    parameters.minimumOnMillis = 2'000U;
    parameters.minimumOffMillis = 1'000U;
    parameters.polarityDeadTimeMillis = 3'000U;
    parameters.pulseAccumulatorCapMillis = 10'000U;
    parameters.counterDirectionConfirmationQuoteThreshold = 0.5;
    parameters.counterDirectionConfirmationDurationMillis = 2'000U;
    parameters.requestWatchdogMillis = 60'000U;
    parameters.outerFanPostRunMillis = 1'000U;
    parameters.innerFanPostRunMillis = 0U;
    return parameters;
}

ControlRequestContext airContext() {
    ControlRequestContext context;
    context.controlSensorRole = ControlSensorRole::Air;
    return context;
}

ControlRequestContext productContext() {
    ControlRequestContext context;
    context.controlSensorRole = ControlSensorRole::Product;
    return context;
}

TemperatureControlResult demandResult(
    AbstractControlDirection direction, double quote, std::uint64_t sequence,
    std::uint64_t createdAt, const ControlRequestContext& context,
    TemperatureControlReason reason = TemperatureControlReason::None,
    AirLimitState airLimitState = AirLimitState::NotApplied) {
    TemperatureControlResult result;
    result.status = TemperatureControlStatus::Demand;
    result.reason = reason;
    result.airLimitState = airLimitState;
    result.direction = direction;
    result.timeQuote = quote;
    ControlRequest request;
    request.identity.sequence = sequence;
    request.identity.createdAtMonotonicMillis = createdAt;
    request.context = context;
    request.direction = direction;
    request.timeQuote = quote;
    result.controlRequest = request;
    return result;
}

TemperatureControlResult offResult(
    std::uint64_t sequence, std::uint64_t createdAt,
    const ControlRequestContext& context,
    TemperatureControlReason reason = TemperatureControlReason::NeutralBand,
    AirLimitState airLimitState = AirLimitState::NotApplied) {
    TemperatureControlResult result;
    result.status = TemperatureControlStatus::Off;
    result.reason = reason;
    result.airLimitState = airLimitState;
    result.direction = AbstractControlDirection::Idle;
    result.timeQuote = 0.0;
    ControlRequest request;
    request.identity.sequence = sequence;
    request.identity.createdAtMonotonicMillis = createdAt;
    request.context = context;
    request.direction = AbstractControlDirection::Idle;
    request.timeQuote = 0.0;
    result.controlRequest = request;
    return result;
}

ActuatorPlanTickInput tickInput(
    std::uint64_t now, std::optional<TemperatureControlResult> evaluation,
    const ControlRequestContext& context) {
    ActuatorPlanTickInput input;
    input.nowMonotonicMillis = now;
    input.newEvaluation = std::move(evaluation);
    input.currentCanonicalContext = context;
    input.temperatureControlledPhase = true;
    input.safetyGate.status = ActuatorSafetyGateStatus::Allowed;
    return input;
}

TemperatureControlResult unavailableResult() {
    TemperatureControlResult result;
    result.status = TemperatureControlStatus::Unavailable;
    result.reason = TemperatureControlReason::SensorUnavailable;
    result.airLimitState = AirLimitState::Unavailable;
    result.direction = AbstractControlDirection::Idle;
    result.timeQuote = 0.0;
    return result;
}

TemperatureControlResult malformedResult() {
    auto result = demandResult(AbstractControlDirection::Heating, 0.5, 0U, 0U,
                               airContext());
    return result;
}

void assertFeedbackUpdate(
    ActuatorPlanner& planner, std::uint64_t sequence,
    PreviousControlRequestFeedback::Disposition disposition) {
    const auto update = planner.takeFeedbackUpdate();
    TEST_ASSERT_TRUE(update.changed);
    TEST_ASSERT_TRUE(update.feedback.has_value());
    TEST_ASSERT_EQUAL_UINT64(sequence, update.feedback->controlRequestSequence);
    TEST_ASSERT_TRUE(update.feedback->disposition == disposition);
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);
}

// --- RZ1: rollenabhaengige AirLimitState-Matrix -----------------------------

void test_product_demand_with_unavailable_airlimitstate_is_malformed() {
    ActuatorPlanner planner{testParameters()};
    const auto eval = demandResult(
        AbstractControlDirection::Heating, 0.5, 1U, 0U, productContext(),
        TemperatureControlReason::None, AirLimitState::Unavailable);
    const auto result = planner.tick(tickInput(0U, eval, productContext()));

    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::MalformedCandidate);
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::InvalidInput);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::MalformedInput);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
}

void test_air_demand_with_air_limit_reduced_reason_is_malformed() {
    ActuatorPlanner planner{testParameters()};
    // ControlSensorRole::Air steuert die Luft direkt; AirLimitReduced ist
    // dort strukturell unmoeglich, unabhaengig vom AirLimitState-Wert.
    const auto eval = demandResult(
        AbstractControlDirection::Heating, 0.5, 1U, 0U, airContext(),
        TemperatureControlReason::AirLimitReduced, AirLimitState::Reduced);
    const auto result = planner.tick(tickInput(0U, eval, airContext()));

    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::MalformedCandidate);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
}

void test_valid_air_and_product_combinations_remain_acceptable() {
    {
        ActuatorPlanner planner{testParameters()};
        const auto eval = demandResult(
            AbstractControlDirection::Heating, 0.5, 1U, 0U, airContext(),
            TemperatureControlReason::None, AirLimitState::NotApplied);
        const auto result = planner.tick(tickInput(0U, eval, airContext()));
        TEST_ASSERT_TRUE(result.admissionOutcome ==
                         ActuatorAdmissionOutcome::Accepted);
    }
    {
        ActuatorPlanner planner{testParameters()};
        const auto eval = demandResult(
            AbstractControlDirection::Cooling, 0.5, 1U, 0U, productContext(),
            TemperatureControlReason::None, AirLimitState::Unrestricted);
        const auto result = planner.tick(tickInput(0U, eval, productContext()));
        TEST_ASSERT_TRUE(result.admissionOutcome ==
                         ActuatorAdmissionOutcome::Accepted);
    }
    {
        ActuatorPlanner planner{testParameters()};
        const auto eval = demandResult(
            AbstractControlDirection::Heating, 0.3, 1U, 0U, productContext(),
            TemperatureControlReason::AirLimitReduced, AirLimitState::Reduced);
        const auto result = planner.tick(tickInput(0U, eval, productContext()));
        TEST_ASSERT_TRUE(result.admissionOutcome ==
                         ActuatorAdmissionOutcome::Accepted);
    }
    {
        ActuatorPlanner planner{testParameters()};
        const auto eval = offResult(1U, 0U, productContext(),
                                    TemperatureControlReason::AirLimitBlocked,
                                    AirLimitState::Blocked);
        const auto result = planner.tick(tickInput(0U, eval, productContext()));
        TEST_ASSERT_TRUE(result.admissionOutcome ==
                         ActuatorAdmissionOutcome::Accepted);
    }
}

// --- RZ2/RZ3/RZ4: Richtungswechsel- und Fensterorakel -----------------------

void test_confirmed_counter_direction_waits_for_arming_before_new_window() {
    ActuatorPlanner planner{testParameters()};

    // t=0: Heating startet mit vollem Puls (scheduledOnMillis == 10000).
    auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);

    // t=500: Gegenrichtung B (Cooling, ueber Schwelle) trifft ein; wirkt vor
    // Ablauf der Mindest-Einschaltzeit physisch nicht (8.5).
    result =
        planner.tick(tickInput(500U,
                               demandResult(AbstractControlDirection::Cooling,
                                            0.8, 2U, 500U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::CounterDirectionConfirming);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());

    // t=2500: Bestaetigungsdauer (2000ms ab t=500) ist erfuellt UND die
    // Mindest-Einschaltzeit der alten Richtung (2000ms ab t=0) ist bereits
    // erfuellt -> sofortiger Teardown der alten Richtung (RZ2-Vorstufe).
    result = planner.tick(tickInput(2'500U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::DirectionChangeApplied);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_TRUE(planner.state().counterDirectionCandidate.has_value());
    TEST_ASSERT_TRUE(*planner.state().counterDirectionCandidate ==
                     AbstractControlDirection::Cooling);

    // t=3000: Minimum-Off (1000ms ab t=2500) noch nicht erfuellt -> weiterhin
    // kein Fenster, Bestaetigung bleibt erhalten (Owner-Review RZ2).
    result = planner.tick(tickInput(3'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::MinimumOffTimeHeld);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(planner.state().counterDirectionConfirmed);

    // t=3600: Minimum-Off (500..1000) erfuellt, Polaritaetstotzeit
    // (1100 < 3000) noch nicht -> weiterhin kein Fenster.
    result = planner.tick(tickInput(3'600U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::PolarityDeadTimeHeld);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());

    // t=5499: eine Millisekunde vor dem spaeteren Gate-Ende (2500+3000)
    // weiterhin kein Fenster.
    result = planner.tick(tickInput(5'499U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::PolarityDeadTimeHeld);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());

    // t=5500: exakt am spaeteren Gate-Ende entsteht das erste B-Fenster; die
    // Bestaetigungsbuchfuehrung wird erst jetzt geloescht.
    result = planner.tick(tickInput(5'500U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Cooling);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::ScheduledWithinWindow);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(planner.state().activeWindow->sourceRequestSequence == 2U);
    TEST_ASSERT_FALSE(planner.state().counterDirectionCandidate.has_value());
}

void test_below_threshold_counter_request_never_gets_a_window_across_old_window_end() {
    ActuatorPlanner planner{testParameters()};

    // t=0: Heating startet mit vollem Puls (scheduledOnMillis == 10000, deckt
    // das gesamte natuerliche Fenster ab).
    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext())));

    // t=1000: Gegenrichtung B liegt UNTER der Umschaltschwelle (0.3 < 0.5) ->
    // kein Kandidat, keine Bestaetigungsfortschreibung.
    auto result =
        planner.tick(tickInput(1'000U,
                               demandResult(AbstractControlDirection::Cooling,
                                            0.3, 2U, 1'000U, airContext()),
                               airContext()));
    TEST_ASSERT_FALSE(planner.state().counterDirectionCandidate.has_value());
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);

    // t=10000: das alte Heating-Fenster erreicht sein natuerliches Ende
    // (Owner-Review RZ3: B darf trotz fehlender Bestaetigung NICHT
    // uebernommen werden).
    result = planner.tick(tickInput(10'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::CounterDirectionConfirming);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());

    // t=15000: weiterhin kein B-Fenster, obwohl laengst physisch Idle.
    result = planner.tick(tickInput(15'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::CounterDirectionConfirming);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());

    // t=16000: B ueberschreitet jetzt erstmals die Schwelle (0.7 >= 0.5) ->
    // die Bestaetigungsdauer beginnt genau jetzt, nicht rueckwirkend.
    result =
        planner.tick(tickInput(16'000U,
                               demandResult(AbstractControlDirection::Cooling,
                                            0.7, 3U, 16'000U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::CounterDirectionConfirming);
    TEST_ASSERT_TRUE(planner.state().counterDirectionCandidate.has_value());
    TEST_ASSERT_FALSE(planner.state().counterDirectionConfirmed);

    // t=17999: noch nicht bestaetigt (1999ms < 2000ms Bestaetigungsdauer).
    result = planner.tick(tickInput(17'999U, std::nullopt, airContext()));
    TEST_ASSERT_FALSE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());

    // t=18000: exakt 2000ms nach Schwellenueberschreitung -> bestaetigt; das
    // Arming (Minimum-Off/Totzeit seit der Deaktivierung bei t=10000) ist zu
    // diesem Zeitpunkt laengst erfuellt, daher entsteht im selben Tick sofort
    // das B-Fenster und die Buchfuehrung wird erwartungsgemaess geloescht.
    result = planner.tick(tickInput(18'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Cooling);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(planner.state().activeWindow->sourceRequestSequence == 3U);
    TEST_ASSERT_FALSE(planner.state().counterDirectionCandidate.has_value());
}

void test_normal_off_portion_of_a_started_pulse_stays_scheduled_never_missed() {
    ActuatorPlanner planner{testParameters()};

    // Quote 0.5 -> scheduledOnMillis == 5000 (direkter Pfad, On-Anteil endet
    // vor dem natuerlichen Fensterende bei 10000).
    auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.5, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::ScheduledWithinWindow);

    result = planner.tick(tickInput(3'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::ScheduledWithinWindow);

    // t=5000: erster Tick im planmaessigen Off-Anteil.
    result = planner.tick(tickInput(5'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::ScheduledWithinWindow);

    // t=7000 und t=9999: weitere Ticks im selben Off-Anteil - Owner-Review
    // RZ4 verlangt hier weiterhin ScheduledWithinWindow, niemals
    // WindowPulseMissed.
    result = planner.tick(tickInput(7'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::ScheduledWithinWindow);

    result = planner.tick(tickInput(9'999U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::ScheduledWithinWindow);
}

void test_genuinely_late_first_tick_reports_window_pulse_missed() {
    ActuatorPlanner planner{testParameters()};

    // t=0: Heating startet (Quote 0.5 -> scheduledOnMillis == 5000). Kein
    // weiterer Tick, bis das Fenster laengst im zweiten Umlauf ist.
    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.5, 1U, 0U, airContext()),
                               airContext())));

    // t=13001: zweites natuerliches Fenster (rebasedStart == 10000) ist
    // bereits 3001ms alt; verbleibende Zeit (1999ms) unterschreitet
    // minimumOnMillis (2000ms) -> Puls wird verworfen, kein Nachholen.
    const auto result =
        planner.tick(tickInput(13'001U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::WindowPulseMissed);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
}

void test_confirmed_counter_direction_arming_gate_is_symmetric_for_cooling() {
    ActuatorPlanner planner{testParameters()};

    // Spiegelbildlich zu test_confirmed_counter_direction_waits_for_arming_
    // before_new_window: Cooling ist die alte Richtung, Heating die
    // bestaetigte Gegenrichtung B (Owner-Review-Punkt "Heating/Cooling
    // spiegelbildlich").
    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Cooling,
                                            1.0, 1U, 0U, airContext()),
                               airContext())));
    static_cast<void>(
        planner.tick(tickInput(500U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.8, 2U, 500U, airContext()),
                               airContext())));

    auto result = planner.tick(tickInput(2'500U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::DirectionChangeApplied);

    result = planner.tick(tickInput(3'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::MinimumOffTimeHeld);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());

    result = planner.tick(tickInput(5'500U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_TRUE(planner.state().activeWindow->sourceRequestSequence == 2U);
}

void test_confirmed_counter_direction_discards_idle_old_window_heating_to_cooling() {
    ActuatorPlanner planner{testParameters()};

    // Heating A is physically off from t=5000, while its natural window still
    // lasts until t=10000. Cooling B starts shortly before that off transition.
    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.5, 1U, 0U, airContext()),
                               airContext())));
    static_cast<void>(
        planner.tick(tickInput(4'000U,
                               demandResult(AbstractControlDirection::Cooling,
                                            0.8, 2U, 4'000U, airContext()),
                               airContext())));

    auto result = planner.tick(tickInput(5'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(
        planner.state().lastPhysicalDeactivationAtMonotonicMillis.has_value());
    TEST_ASSERT_TRUE(
        *planner.state().lastPhysicalDeactivationAtMonotonicMillis == 5'000U);

    // B confirms after the old physical pulse is already off, before the old
    // natural window ends. The old snapshot disappears in this tick and the
    // confirmation remains the sole B identity.
    result = planner.tick(tickInput(6'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::PolarityDeadTimeHeld);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_TRUE(planner.state().counterDirectionCandidate.has_value());
    TEST_ASSERT_FALSE(result.counterDirectionConfirming);

    result = planner.tick(tickInput(7'999U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::PolarityDeadTimeHeld);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());

    // The later gate ends at t=8000 (dead time from the real t=5000
    // deactivation), not at the old natural window boundary t=10000.
    result = planner.tick(tickInput(8'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Cooling);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(planner.state().activeWindow->sourceRequestSequence == 2U);
    TEST_ASSERT_FALSE(planner.state().counterDirectionCandidate.has_value());
}

void test_confirmed_counter_direction_discards_idle_old_window_cooling_to_heating() {
    ActuatorPlanner planner{testParameters()};

    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Cooling,
                                            0.5, 1U, 0U, airContext()),
                               airContext())));
    static_cast<void>(
        planner.tick(tickInput(4'000U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.8, 2U, 4'000U, airContext()),
                               airContext())));
    static_cast<void>(
        planner.tick(tickInput(5'000U, std::nullopt, airContext())));

    const auto result =
        planner.tick(tickInput(6'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::PolarityDeadTimeHeld);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
    TEST_ASSERT_FALSE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_TRUE(planner.state().counterDirectionCandidate.has_value());

    const auto gateResult =
        planner.tick(tickInput(8'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(gateResult.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(gateResult.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
    TEST_ASSERT_TRUE(planner.state().activeWindow->sourceRequestSequence == 2U);
    TEST_ASSERT_FALSE(planner.state().counterDirectionCandidate.has_value());
}

void test_neutral_off_interrupts_counter_confirmation_during_minimum_on() {
    ActuatorPlanner planner{testParameters()};

    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext())));
    static_cast<void>(
        planner.tick(tickInput(500U,
                               demandResult(AbstractControlDirection::Cooling,
                                            0.8, 2U, 500U, airContext()),
                               airContext())));

    // The fresh NeutralOff request must clear B's confirmation bookkeeping
    // before the old Heating pulse is held through minimum-on.
    auto result = planner.tick(
        tickInput(1'000U, offResult(3U, 1'000U, airContext()), airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::MinimumOnTimeHeld);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_FALSE(planner.state().counterDirectionCandidate.has_value());
    TEST_ASSERT_TRUE(
        planner.state().counterDirectionObservedSinceMonotonicMillis == 0U);
    TEST_ASSERT_FALSE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());

    // B returns before the old minimum-on phase ends: its observation starts
    // anew at t=1500, never at the interrupted t=500 timestamp.
    result =
        planner.tick(tickInput(1'500U,
                               demandResult(AbstractControlDirection::Cooling,
                                            0.8, 4U, 1'500U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_FALSE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_TRUE(planner.state().counterDirectionCandidate.has_value());
    TEST_ASSERT_TRUE(*planner.state().counterDirectionCandidate ==
                     AbstractControlDirection::Cooling);
    TEST_ASSERT_TRUE(
        planner.state().counterDirectionObservedSinceMonotonicMillis == 1'500U);

    result = planner.tick(tickInput(3'499U, std::nullopt, airContext()));
    TEST_ASSERT_FALSE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
    result = planner.tick(tickInput(3'500U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::DirectionChangeApplied);
}

void test_parameter_classification_covers_all_structural_relations() {
    const auto valid = testParameters();
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(valid) ==
                     ActuatorPlannerParametersValidation::Valid);
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters({}) ==
                     ActuatorPlannerParametersValidation::Unconfigured);

    auto invalid = valid;
    invalid.switchingWindowMillis = 0U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.minimumOnMillis = 0U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.minimumOnMillis = invalid.switchingWindowMillis + 1U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.minimumOffMillis = 0U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.polarityDeadTimeMillis = 0U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.pulseAccumulatorCapMillis = invalid.minimumOnMillis - 1U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.counterDirectionConfirmationQuoteThreshold = 0.0;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.counterDirectionConfirmationQuoteThreshold = 1.1;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.counterDirectionConfirmationDurationMillis = 0U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.requestWatchdogMillis = 0U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.outerFanPostRunMillis = 0U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);
    invalid = valid;
    invalid.switchingWindowMillis = 9'007'199'254'740'993ULL;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(invalid) ==
                     ActuatorPlannerParametersValidation::Invalid);

    auto noInnerPostRun = valid;
    noInnerPostRun.innerFanPostRunMillis = 0U;
    TEST_ASSERT_TRUE(classifyActuatorPlannerParameters(noInnerPostRun) ==
                     ActuatorPlannerParametersValidation::Valid);
}

void test_window_ownership_and_variant_b_feedback_are_separate() {
    ActuatorPlanner planner{testParameters()};
    auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.8, 10U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        10U, planner.state().activeWindow->sourceRequestSequence);
    static_cast<void>(planner.takeFeedbackUpdate());

    // B is a same-direction synchronization request. It must not rewrite the
    // physical ownership snapshot of A, and it is not itself downstream
    // limited merely because A's window is still running.
    result =
        planner.tick(tickInput(100U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.3, 11U, 100U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::ScheduledWithinWindow);
    TEST_ASSERT_EQUAL_UINT64(
        10U, planner.state().activeWindow->sourceRequestSequence);
    assertFeedbackUpdate(
        planner, 11U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    result = planner.tick(tickInput(10'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        11U, planner.state().activeWindow->sourceRequestSequence);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
}

void test_fail_closed_matrix_stops_physical_output_and_trusts_only_sequences() {
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        const auto result = planner.tick(ActuatorPlanTickInput{
            500U, std::nullopt, airContext(), true,
            ActuatorSafetyGateInput{ActuatorSafetyGateStatus::ImmediateStop}});
        TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
        TEST_ASSERT_TRUE(result.appliedDirection ==
                         AbstractControlDirection::Idle);
        assertFeedbackUpdate(
            planner, 1U, PreviousControlRequestFeedback::Disposition::Rejected);
    }
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        auto malformedGate = ActuatorSafetyGateInput{
            static_cast<ActuatorSafetyGateStatus>(0xFF)};
        const auto result = planner.tick(ActuatorPlanTickInput{
            500U, std::nullopt, airContext(), true, malformedGate});
        TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::InvalidInput);
        TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::MalformedInput);
        assertFeedbackUpdate(
            planner, 1U, PreviousControlRequestFeedback::Disposition::Rejected);
    }
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        const auto result = planner.tick(ActuatorPlanTickInput{
            500U, unavailableResult(), airContext(), true,
            ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed}});
        TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::NoValidRequest);
        TEST_ASSERT_TRUE(result.appliedDirection ==
                         AbstractControlDirection::Idle);
        const auto update = planner.takeFeedbackUpdate();
        TEST_ASSERT_TRUE(update.changed);
        TEST_ASSERT_FALSE(update.feedback.has_value());
    }
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(100U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   100U, airContext()),
                      airContext())));
        const auto result = planner.tick(ActuatorPlanTickInput{
            50U, std::nullopt, airContext(), true,
            ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed}});
        TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::TimeInvalid);
        TEST_ASSERT_TRUE(result.appliedDirection ==
                         AbstractControlDirection::Idle);
        assertFeedbackUpdate(
            planner, 1U, PreviousControlRequestFeedback::Disposition::Rejected);
    }
    {
        ActuatorPlannerParameters parameters = testParameters();
        parameters.requestWatchdogMillis = 500U;
        ActuatorPlanner planner{parameters};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        const auto result =
            planner.tick(tickInput(500U, std::nullopt, airContext()));
        TEST_ASSERT_TRUE(result.reason ==
                         ActuatorPlanReason::StaleRequestWatchdog);
        TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());
        assertFeedbackUpdate(
            planner, 1U, PreviousControlRequestFeedback::Disposition::Rejected);
    }
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        const auto result = planner.tick(ActuatorPlanTickInput{
            500U, std::nullopt, productContext(), true,
            ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed}});
        TEST_ASSERT_TRUE(result.reason ==
                         ActuatorPlanReason::StaleRequestContext);
        assertFeedbackUpdate(
            planner, 1U, PreviousControlRequestFeedback::Disposition::Rejected);
    }
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        const auto result =
            planner.tick(tickInput(500U, malformedResult(), airContext()));
        TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::InvalidInput);
        TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::MalformedInput);
        const auto update = planner.takeFeedbackUpdate();
        TEST_ASSERT_TRUE(update.changed);
        TEST_ASSERT_FALSE(update.feedback.has_value());
    }
    // Owner-Review R2 (Plan-19.2 #5): I-3a (SafetyGateUnresolved) und I-4
    // (RequestWatchdogFaultLatched) ergaenzen die Matrix, jeweils innerhalb
    // einer laufenden Mindest-On-Zeit, mit explizitem Nachweis des
    // physischen Deaktivierungsankers.
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        const auto result = planner.tick(ActuatorPlanTickInput{
            500U, std::nullopt, airContext(), true,
            ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Unresolved}});
        TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
        TEST_ASSERT_TRUE(result.reason ==
                         ActuatorPlanReason::SafetyGateUnresolved);
        TEST_ASSERT_TRUE(result.appliedDirection ==
                         AbstractControlDirection::Idle);
        TEST_ASSERT_TRUE(
            planner.state()
                .lastPhysicalDeactivationAtMonotonicMillis.has_value());
        TEST_ASSERT_EQUAL_UINT64(
            500U, *planner.state().lastPhysicalDeactivationAtMonotonicMillis);
        assertFeedbackUpdate(
            planner, 1U, PreviousControlRequestFeedback::Disposition::Rejected);
    }
    {
        ActuatorPlannerParameters parameters = testParameters();
        parameters.requestWatchdogMillis = 500U;
        ActuatorPlanner planner{parameters};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        // t=500: laufender Watchdog trippt (I-5) und latcht den Fehler.
        static_cast<void>(
            planner.tick(tickInput(500U, std::nullopt, airContext())));
        static_cast<void>(planner.takeFeedbackUpdate());
        TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());
        // t=600: ein weiterer Tick ohne neue Evaluation faellt jetzt auf I-4
        // (bereits gelatchter Fehler), nicht mehr auf I-5. Owner-Review R2:
        // anders als I-1/I-3a/I-3b/I-5/I-6/I-7/I-8 kann I-4 den physischen
        // Ausgang strukturell NIE selbst von aktiv auf Idle schalten - der
        // Fehler wurde bereits im selben oder einem frueheren Tick durch I-5
        // gelatcht, welches den Ausgang schon abgeschaltet hat, und Klasse
        // I-4 verhindert in der Prioritaetsleiter jede spaetere
        // Kandidatenuebernahme, die ihn erneut aktiv machen koennte. Der
        // Deaktivierungsanker ist deshalb hier bereits vom I-5-Tick gesetzt,
        // nicht neu von diesem I-4-Tick.
        const auto result =
            planner.tick(tickInput(600U, std::nullopt, airContext()));
        TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Idle);
        TEST_ASSERT_TRUE(result.reason ==
                         ActuatorPlanReason::RequestWatchdogFaultLatched);
        TEST_ASSERT_TRUE(result.appliedDirection ==
                         AbstractControlDirection::Idle);
        TEST_ASSERT_TRUE(
            planner.state()
                .lastPhysicalDeactivationAtMonotonicMillis.has_value());
    }
}

// Owner-Review R2 (Plan-19.2 #5): I-2a (Unconfigured) und I-2b (Invalid)
// koennen strukturell NICHT "innerhalb einer laufenden Mindest-On-Zeit"
// geprueft werden - parameters_ ist pro Planner-Instanz unveraenderlich und
// wird bei JEDEM Tick klassifiziert (Klasse I-2a/I-2b liegt in der
// Prioritaetsleiter vor jeder Fensteruebernahme), sodass ein Fenster unter
// Unconfigured/Invalid-Parametern niemals entstehen kann. Der einzig
// moegliche, ehrliche Nachweis ist der sofortige Fail-closed-Verwurf bereits
// im ersten Tick.
void test_i2a_unconfigured_parameters_reject_immediately() {
    ActuatorPlanner planner{ActuatorPlannerParameters{}};
    const auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Unconfigured);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::NoCommissioning);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
    assertFeedbackUpdate(planner, 1U,
                         PreviousControlRequestFeedback::Disposition::Rejected);
}

void test_i2b_invalid_parameters_reject_immediately() {
    auto parameters = testParameters();
    parameters.minimumOnMillis = parameters.switchingWindowMillis + 1U;
    ActuatorPlanner planner{parameters};
    const auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::InvalidInput);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::InvalidConfiguration);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
    assertFeedbackUpdate(planner, 1U,
                         PreviousControlRequestFeedback::Disposition::Rejected);
}

void test_no_valid_request_and_off_close_planner_without_old_feedback() {
    for (const auto& result :
         {unavailableResult(),
          offResult(2U, 500U, airContext(),
                    TemperatureControlReason::NeutralBand,
                    AirLimitState::NotApplied),
          offResult(2U, 500U, productContext(),
                    TemperatureControlReason::AirLimitBlocked,
                    AirLimitState::Blocked)}) {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(0U,
                      demandResult(AbstractControlDirection::Heating, 0.5, 1U,
                                   0U, airContext()),
                      airContext())));
        static_cast<void>(planner.takeFeedbackUpdate());
        const auto stopped = planner.tick(tickInput(
            500U, result,
            result.controlRequest.has_value() ? result.controlRequest->context
                                              : airContext()));
        if (result.status == TemperatureControlStatus::Unavailable) {
            TEST_ASSERT_TRUE(stopped.appliedDirection ==
                             AbstractControlDirection::Idle);
        } else {
            TEST_ASSERT_TRUE(stopped.status == ActuatorPlanStatus::Active);
            TEST_ASSERT_TRUE(stopped.reason ==
                             ActuatorPlanReason::MinimumOnTimeHeld);
        }
        const auto update = planner.takeFeedbackUpdate();
        TEST_ASSERT_TRUE(update.changed);
        TEST_ASSERT_FALSE(update.feedback.has_value());
    }
}

void test_feedback_disposition_maps_reasons_and_never_downgrades() {
    ActuatorPlanner planner{testParameters()};
    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.3, 1U, 0U, airContext()),
                               airContext())));
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    // A real counter-direction gate is deferred, while the previous
    // sequence's no-constraint observation is never replayed or downgraded.
    const auto result =
        planner.tick(tickInput(500U,
                               demandResult(AbstractControlDirection::Cooling,
                                            0.8, 2U, 500U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::CounterDirectionConfirming);
    assertFeedbackUpdate(
        planner, 2U,
        PreviousControlRequestFeedback::Disposition::DeferredOrLimited);

    static_cast<void>(
        planner.tick(tickInput(2'500U, std::nullopt, airContext())));
    const auto update = planner.takeFeedbackUpdate();
    TEST_ASSERT_FALSE(update.changed);
}

void test_malformed_evaluation_clears_pending_and_allows_next_sequence() {
    ActuatorPlanner planner{testParameters()};
    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 10U, 0U, airContext()),
                               airContext())));
    static_cast<void>(planner.takeFeedbackUpdate());
    auto result =
        planner.tick(tickInput(100U, malformedResult(), airContext()));
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::MalformedInput);
    const auto malformedUpdate = planner.takeFeedbackUpdate();
    TEST_ASSERT_TRUE(malformedUpdate.changed);
    TEST_ASSERT_FALSE(malformedUpdate.feedback.has_value());

    result =
        planner.tick(tickInput(200U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 11U, 200U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::Accepted);
    TEST_ASSERT_TRUE(result.acceptedCommandSequence.has_value());
    TEST_ASSERT_EQUAL_UINT64(11U, *result.acceptedCommandSequence);
}

void test_feedback_episode_close_prevents_force_stop_resurrection() {
    ActuatorPlanner planner{testParameters()};
    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 10U, 0U, airContext()),
                               airContext())));
    static_cast<void>(planner.takeFeedbackUpdate());
    planner.closeFeedbackEpisodeForOutstandingEvaluation();
    const auto update = planner.takeFeedbackUpdate();
    TEST_ASSERT_TRUE(update.changed);
    TEST_ASSERT_FALSE(update.feedback.has_value());
}

// --- Owner-Review F1: feedback episode ownership vs. stale physical
// governance. An old acceptedCommand may keep running physically
// (minimum-on/window) after its own feedback episode already closed via a
// newer evaluation; it must never be able to write into pendingFeedback
// again, not even across a later tick with no new evaluation at all. ---

void test_stale_on_arrival_watchdog_b_closes_episode_a_never_resurrects() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 5'000U;
    ActuatorPlanner planner{parameters};

    // A: valid active Heating admitted far from t=0 so B's own stale
    // identity (createdAt=0) does not also trip the running H-watchdog.
    auto result =
        planner.tick(tickInput(1'000'000U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 1'000'000U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    // B: structurally valid, active, but already older than
    // requestWatchdogMillis when it arrives -> StaleOnArrivalWatchdog. A's
    // own H (set at t=1'000'000) is far from expiring, so this must not
    // also trip the running I-5 watchdog.
    result =
        planner.tick(tickInput(1'000'500U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 2U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::StaleOnArrivalWatchdog);
    // A continues to run physically per ordinary window governance in the
    // very same tick that closed and rejected B.
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    assertFeedbackUpdate(planner, 2U,
                         PreviousControlRequestFeedback::Disposition::Rejected);

    // A further tick with no new evaluation is the case a per-tick-only
    // derivation would miss: A keeps governing physically, but must not be
    // able to write feedback again.
    result = planner.tick(tickInput(1'001'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);
}

void test_stale_on_arrival_context_b_closes_episode_a_never_resurrects() {
    ActuatorPlanner planner{testParameters()};

    auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    // B: structurally valid, active, fresh in time, but its own context does
    // not match the current canonical context -> StaleOnArrivalContext. A's
    // own accepted context (airContext()) still matches, so I-7 must not
    // fire either.
    result = planner.tick(
        tickInput(500U,
                  demandResult(AbstractControlDirection::Heating, 1.0, 2U, 500U,
                               productContext(), TemperatureControlReason::None,
                               AirLimitState::Unrestricted),
                  airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::StaleOnArrivalContext);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    assertFeedbackUpdate(planner, 2U,
                         PreviousControlRequestFeedback::Disposition::Rejected);

    result = planner.tick(tickInput(1'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);
}

void test_new_stale_off_request_closes_episode_without_any_a_feedback() {
    ActuatorPlanner planner{testParameters()};

    auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    // A structurally valid OFF-classified evaluation that is itself
    // stale-on-arrival (context mismatch): it must close A's episode without
    // ever opening a feedback window of its own, since #22 never opens a
    // feedback window for OFF (Abschnitt 6.2 Schritt 6c / ZR5).
    result =
        planner.tick(tickInput(500U,
                               offResult(2U, 500U, productContext(),
                                         TemperatureControlReason::NeutralBand,
                                         AirLimitState::Unrestricted),
                               airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::StaleOnArrivalContext);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    const auto closingUpdate = planner.takeFeedbackUpdate();
    TEST_ASSERT_TRUE(closingUpdate.changed);
    TEST_ASSERT_FALSE(closingUpdate.feedback.has_value());

    result = planner.tick(tickInput(1'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);
}

void test_duplicate_evaluation_does_not_disturb_pending_feedback_or_resurrect_a() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 5'000U;
    ActuatorPlanner planner{parameters};

    auto result =
        planner.tick(tickInput(1'000'000U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 1'000'000U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    // B: stale-on-arrival, closes A's episode and opens {2, Rejected} - left
    // deliberately unconsumed by this test.
    result =
        planner.tick(tickInput(1'000'500U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 2U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::StaleOnArrivalWatchdog);
    TEST_ASSERT_TRUE(planner.state().pendingFeedback.has_value());
    TEST_ASSERT_EQUAL_UINT64(2U, planner.state().pendingFeedback->sequence);

    // A byte-identical replay of A's own already-superseded sequence must
    // leave the still-unconsumed {2, Rejected} completely untouched: it is
    // neither reset to nullopt (6.2 Punkt 5 / 9.2) nor does A's continued
    // physical governance get a chance to overwrite it (F1).
    result =
        planner.tick(tickInput(1'001'000U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::DuplicateOrOldSequence);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_TRUE(planner.state().pendingFeedback.has_value());
    TEST_ASSERT_EQUAL_UINT64(2U, planner.state().pendingFeedback->sequence);
    TEST_ASSERT_TRUE(planner.state().pendingFeedback->disposition ==
                     PreviousControlRequestFeedback::Disposition::Rejected);
    assertFeedbackUpdate(planner, 2U,
                         PreviousControlRequestFeedback::Disposition::Rejected);
}

// --- Owner-Review R1: feedbackEpisodeSubjectSequence is the sole authority
// for rejectToIdle()/forceStop() too, not just mergeFeedbackForDemand(). A
// stale acceptedCommand must never resurface via these paths either,
// including across a tick separate from the one that closed its episode. ---

void test_i_event_after_stale_b_never_resurrects_a_immediate_stop() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 5'000U;
    ActuatorPlanner planner{parameters};

    auto result =
        planner.tick(tickInput(1'000'000U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 1'000'000U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    result =
        planner.tick(tickInput(1'000'500U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 2U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::StaleOnArrivalWatchdog);
    assertFeedbackUpdate(planner, 2U,
                         PreviousControlRequestFeedback::Disposition::Rejected);

    // An unconditional fail-closed safety event with no new evaluation, on a
    // tick separate from B's own admission tick, must only ever sharpen B's
    // still-open subject - never resurrect A via a stale
    // acceptedCommand-based fallback.
    const auto stopResult = planner.tick(ActuatorPlanTickInput{
        1'001'000U, std::nullopt, airContext(), true,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::ImmediateStop}});
    TEST_ASSERT_TRUE(stopResult.status == ActuatorPlanStatus::Idle);
    TEST_ASSERT_TRUE(stopResult.appliedDirection ==
                     AbstractControlDirection::Idle);
    // B's {2, Rejected} was already consumed and is unchanged (same
    // severity) - no new update, and certainly never {1, ...}.
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);
    TEST_ASSERT_TRUE(
        planner.state().feedbackEpisodeSubjectSequence.has_value());
    TEST_ASSERT_EQUAL_UINT64(2U,
                             *planner.state().feedbackEpisodeSubjectSequence);
}

void test_i_event_after_stale_b_never_resurrects_a_running_watchdog() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 1'000U;
    ActuatorPlanner planner{parameters};

    // A: admitted just inside its own watchdog boundary (999 < 1000), so H
    // is set to 999 rather than to A's own createdAt (0). This is the only
    // way to trip B's own admission-staleness boundary (below) without the
    // running watchdog also tripping in the very same tick.
    auto result =
        planner.tick(tickInput(999U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    // B: stale-on-arrival exactly at its own boundary (1000 - 0 = 1000),
    // while H's own boundary (999 + 1000 = 1999) is not yet due.
    result =
        planner.tick(tickInput(1'000U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 2U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::StaleOnArrivalWatchdog);
    assertFeedbackUpdate(planner, 2U,
                         PreviousControlRequestFeedback::Disposition::Rejected);

    // A separate later tick, still with no new evaluation, now trips the
    // running watchdog (I-5) itself - a genuinely different I-event class
    // than the previous test's external safety override.
    const auto watchdogResult =
        planner.tick(tickInput(2'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(watchdogResult.reason ==
                     ActuatorPlanReason::StaleRequestWatchdog);
    TEST_ASSERT_TRUE(watchdogResult.appliedDirection ==
                     AbstractControlDirection::Idle);
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);
    TEST_ASSERT_TRUE(
        planner.state().feedbackEpisodeSubjectSequence.has_value());
    TEST_ASSERT_EQUAL_UINT64(2U,
                             *planner.state().feedbackEpisodeSubjectSequence);
}

void test_repeated_i4_tick_after_watchdog_trip_does_not_erase_pending_rejected() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 5'000U;
    ActuatorPlanner planner{parameters};

    auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext()));
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    // No further evaluation; the running watchdog trips on H's own boundary.
    result = planner.tick(tickInput(5'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::StaleRequestWatchdog);
    TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());
    TEST_ASSERT_TRUE(planner.state().pendingFeedback.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, planner.state().pendingFeedback->sequence);
    TEST_ASSERT_TRUE(planner.state().pendingFeedback->disposition ==
                     PreviousControlRequestFeedback::Disposition::Rejected);

    // Owner-Review R1b: a further tick before the next #22 evaluation hits
    // the already-latched fault (I-4). It must not erase the still-
    // unconsumed {1, Rejected} to nullopt, and must not downgrade it.
    result = planner.tick(tickInput(5'500U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::RequestWatchdogFaultLatched);
    TEST_ASSERT_TRUE(planner.state().pendingFeedback.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, planner.state().pendingFeedback->sequence);
    TEST_ASSERT_TRUE(planner.state().pendingFeedback->disposition ==
                     PreviousControlRequestFeedback::Disposition::Rejected);
    assertFeedbackUpdate(planner, 1U,
                         PreviousControlRequestFeedback::Disposition::Rejected);
}

void test_force_stop_existing_episode_open_uses_current_subject_not_stale_accepted_command() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 5'000U;
    ActuatorPlanner planner{parameters};

    auto result =
        planner.tick(tickInput(1'000'000U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 1'000'000U, airContext()),
                               airContext()));
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    result =
        planner.tick(tickInput(1'000'500U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 2U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.admissionOutcome ==
                     ActuatorAdmissionOutcome::StaleOnArrivalWatchdog);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    // {2, Rejected} deliberately left unconsumed here.

    const auto stopResult = planner.forceStop(
        1'001'000U, ActuatorFeedbackEpisodeAtStop::ExistingEpisodeOpen);
    TEST_ASSERT_TRUE(stopResult.appliedDirection ==
                     AbstractControlDirection::Idle);
    assertFeedbackUpdate(planner, 2U,
                         PreviousControlRequestFeedback::Disposition::Rejected);
}

void test_force_stop_closed_by_outstanding_evaluation_never_emits_old_feedback() {
    ActuatorPlanner planner{testParameters()};
    static_cast<void>(
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 1U, 0U, airContext()),
                               airContext())));
    assertFeedbackUpdate(
        planner, 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    const auto stopResult = planner.forceStop(
        1'000U, ActuatorFeedbackEpisodeAtStop::ClosedByOutstandingEvaluation);
    TEST_ASSERT_TRUE(stopResult.appliedDirection ==
                     AbstractControlDirection::Idle);
    const auto update = planner.takeFeedbackUpdate();
    TEST_ASSERT_TRUE(update.changed);
    TEST_ASSERT_FALSE(update.feedback.has_value());
}

// Owner-Review R2 (Plan-19.2 #14): H (lastNewRequestAcceptedAtMonotonicMillis)
// wird ausschliesslich fuer jede neue, vertrauenswuerdige HEAT-/COOL-/OFF-/
// NoValidRequest-Evaluation aktualisiert; Replay, strukturell malformed
// Evaluation und stale-on-arrival Watchdog/Context ruehren es nicht an.
void test_watchdog_heartbeat_updates_only_for_new_valid_evaluations() {
    // Positiv: HEAT.
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(100U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   100U, airContext()),
                      airContext())));
        TEST_ASSERT_TRUE(
            planner.state()
                .lastNewRequestAcceptedAtMonotonicMillis.has_value());
        TEST_ASSERT_EQUAL_UINT64(
            100U, *planner.state().lastNewRequestAcceptedAtMonotonicMillis);
    }
    // Positiv: COOL.
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(100U,
                      demandResult(AbstractControlDirection::Cooling, 1.0, 1U,
                                   100U, airContext()),
                      airContext())));
        TEST_ASSERT_TRUE(
            planner.state()
                .lastNewRequestAcceptedAtMonotonicMillis.has_value());
        TEST_ASSERT_EQUAL_UINT64(
            100U, *planner.state().lastNewRequestAcceptedAtMonotonicMillis);
    }
    // Positiv: OFF (NeutralOff).
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(100U, offResult(1U, 100U, airContext()), airContext())));
        TEST_ASSERT_TRUE(
            planner.state()
                .lastNewRequestAcceptedAtMonotonicMillis.has_value());
        TEST_ASSERT_EQUAL_UINT64(
            100U, *planner.state().lastNewRequestAcceptedAtMonotonicMillis);
    }
    // Positiv: NoValidRequest (schaltet Peltier aus, setzt H, kein
    // zusaetzlicher Staleness-Trip).
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(
            planner.tick(tickInput(100U, unavailableResult(), airContext())));
        TEST_ASSERT_TRUE(
            planner.state()
                .lastNewRequestAcceptedAtMonotonicMillis.has_value());
        TEST_ASSERT_EQUAL_UINT64(
            100U, *planner.state().lastNewRequestAcceptedAtMonotonicMillis);
        const auto follow =
            planner.tick(tickInput(200U, std::nullopt, airContext()));
        TEST_ASSERT_FALSE(planner.state().latchedWatchdogFault.has_value());
        TEST_ASSERT_FALSE(follow.reason ==
                          ActuatorPlanReason::StaleRequestWatchdog);
    }
    // Negativ: Replay ruehrt H nicht an.
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(100U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 5U,
                                   100U, airContext()),
                      airContext())));
        static_cast<void>(planner.tick(
            tickInput(300U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 5U,
                                   100U, airContext()),
                      airContext())));
        TEST_ASSERT_EQUAL_UINT64(
            100U, *planner.state().lastNewRequestAcceptedAtMonotonicMillis);
    }
    // Negativ: strukturell malformed Evaluation ruehrt H nicht an.
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(100U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   100U, airContext()),
                      airContext())));
        static_cast<void>(
            planner.tick(tickInput(300U, malformedResult(), airContext())));
        TEST_ASSERT_EQUAL_UINT64(
            100U, *planner.state().lastNewRequestAcceptedAtMonotonicMillis);
    }
    // Negativ: stale-on-arrival Watchdog ruehrt H nicht an. Timing isoliert
    // von H's eigener laufender Frist (H=999, B's eigene Staleness trippt
    // exakt bei 1000, waehrend H's Grenze 1999 noch nicht faellig ist).
    {
        auto parameters = testParameters();
        parameters.requestWatchdogMillis = 1'000U;
        ActuatorPlanner planner{parameters};
        static_cast<void>(planner.tick(
            tickInput(999U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   0U, airContext()),
                      airContext())));
        static_cast<void>(planner.tick(
            tickInput(1'000U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 2U,
                                   0U, airContext()),
                      airContext())));
        TEST_ASSERT_EQUAL_UINT64(
            999U, *planner.state().lastNewRequestAcceptedAtMonotonicMillis);
    }
    // Negativ: stale-on-arrival Context ruehrt H nicht an.
    {
        ActuatorPlanner planner{testParameters()};
        static_cast<void>(planner.tick(
            tickInput(100U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   100U, airContext()),
                      airContext())));
        static_cast<void>(planner.tick(
            tickInput(300U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 2U,
                                   300U, productContext()),
                      airContext())));
        TEST_ASSERT_EQUAL_UINT64(
            100U, *planner.state().lastNewRequestAcceptedAtMonotonicMillis);
    }
}

// Owner-Review R2 (Plan-19.2 #25): der allererste Planner-Tick einer neuen
// ueberwachten Episode setzt den Episodenanker allein durch
// temperatureControlledPhase, ohne jede Evaluation; knapp vor/exakt auf/nach
// der Watchdogfrist wird der Trip deterministisch geprueft. Ein erstes H
// rebased die Frist auf sich selbst statt auf den Episodenanker.
void test_watchdog_episode_anchor_bootstraps_and_h_rebases_deadline() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 1'000U;

    {
        ActuatorPlanner planner{parameters};
        static_cast<void>(
            planner.tick(tickInput(0U, std::nullopt, airContext())));
        TEST_ASSERT_TRUE(
            planner.state()
                .watchdogEpisodeStartedAtMonotonicMillis.has_value());
        TEST_ASSERT_EQUAL_UINT64(
            0U, *planner.state().watchdogEpisodeStartedAtMonotonicMillis);
        TEST_ASSERT_FALSE(planner.state().latchedWatchdogFault.has_value());
    }
    {
        ActuatorPlanner planner{parameters};
        static_cast<void>(
            planner.tick(tickInput(0U, std::nullopt, airContext())));
        const auto result =
            planner.tick(tickInput(999U, std::nullopt, airContext()));
        TEST_ASSERT_FALSE(result.reason ==
                          ActuatorPlanReason::StaleRequestWatchdog);
        TEST_ASSERT_FALSE(planner.state().latchedWatchdogFault.has_value());
    }
    {
        ActuatorPlanner planner{parameters};
        static_cast<void>(
            planner.tick(tickInput(0U, std::nullopt, airContext())));
        const auto result =
            planner.tick(tickInput(1'000U, std::nullopt, airContext()));
        TEST_ASSERT_TRUE(result.reason ==
                         ActuatorPlanReason::StaleRequestWatchdog);
        TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());
    }
    {
        ActuatorPlanner planner{parameters};
        static_cast<void>(
            planner.tick(tickInput(0U, std::nullopt, airContext())));
        const auto result =
            planner.tick(tickInput(1'001U, std::nullopt, airContext()));
        TEST_ASSERT_TRUE(result.reason ==
                         ActuatorPlanReason::StaleRequestWatchdog);
    }
    {
        ActuatorPlanner planner{parameters};
        static_cast<void>(
            planner.tick(tickInput(0U, std::nullopt, airContext())));
        static_cast<void>(planner.tick(
            tickInput(500U,
                      demandResult(AbstractControlDirection::Heating, 1.0, 1U,
                                   500U, airContext()),
                      airContext())));
        // Alte Anker-Deadline (0+1000=1000) waere hier bereits ueberschritten;
        // die durch H rebased Deadline (500+1000=1500) ist es nicht.
        const auto result =
            planner.tick(tickInput(1'000U, std::nullopt, airContext()));
        TEST_ASSERT_FALSE(result.reason ==
                          ActuatorPlanReason::StaleRequestWatchdog);
        TEST_ASSERT_FALSE(planner.state().latchedWatchdogFault.has_value());
    }
}

// Owner-Review R2 (Plan-19.2 #26): eine lange Standby-/Service-Zeit und
// danach ein neuer Episodeneintritt (NewActiveRun/Recovery) darf keinen
// Soforttrip aus einem alten H-/Episodenanker erzeugen - forceStop() rebased
// beide auf den naechsten Episodenanker; latchedWatchdogFault bleibt ueber
// den Boundary hinweg erhalten, weil ausschliesslich #24 ueber
// applyExternalWatchdogFaultReset() es loeschen darf.
void test_forcestop_rebase_after_long_standby_does_not_trip_and_preserves_latch() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 1'000U;
    ActuatorPlanner planner{parameters};

    static_cast<void>(planner.tick(tickInput(0U, std::nullopt, airContext())));
    const auto tripResult =
        planner.tick(tickInput(1'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(tripResult.reason ==
                     ActuatorPlanReason::StaleRequestWatchdog);
    TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());
    static_cast<void>(planner.takeFeedbackUpdate());

    static_cast<void>(planner.forceStop(
        1'000U, ActuatorFeedbackEpisodeAtStop::ExistingEpisodeOpen));
    static_cast<void>(planner.takeFeedbackUpdate());
    TEST_ASSERT_FALSE(
        planner.state().watchdogEpisodeStartedAtMonotonicMillis.has_value());
    TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());

    const auto reentry =
        planner.tick(tickInput(1'000'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(
        planner.state().watchdogEpisodeStartedAtMonotonicMillis.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        1'000'000U, *planner.state().watchdogEpisodeStartedAtMonotonicMillis);
    TEST_ASSERT_TRUE(reentry.reason ==
                     ActuatorPlanReason::RequestWatchdogFaultLatched);
    TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());
}

// Owner-Review R2 (Plan-19.2 #28): A=10 wird gueltig angenommen, B=11
// stale-on-arrival verworfen, danach trippt der laufende Watchdog separat.
// Das Evidence-Orakel prueft das ehrlich benannte
// lastObservedSequenceHighWatermarkBeforeFault == 11 (B wurde beobachtet,
// nicht angenommen) statt einer falschen lastAccepted-Semantik.
void test_watchdog_fault_evidence_reports_high_watermark_not_last_accepted() {
    auto parameters = testParameters();
    parameters.requestWatchdogMillis = 1'000U;
    ActuatorPlanner planner{parameters};

    static_cast<void>(
        planner.tick(tickInput(999U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 10U, 0U, airContext()),
                               airContext())));
    assertFeedbackUpdate(
        planner, 10U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint);

    static_cast<void>(
        planner.tick(tickInput(1'000U,
                               demandResult(AbstractControlDirection::Heating,
                                            1.0, 11U, 0U, airContext()),
                               airContext())));
    assertFeedbackUpdate(planner, 11U,
                         PreviousControlRequestFeedback::Disposition::Rejected);

    const auto tripResult =
        planner.tick(tickInput(1'999U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(tripResult.reason ==
                     ActuatorPlanReason::StaleRequestWatchdog);
    TEST_ASSERT_TRUE(planner.state().latchedWatchdogFault.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        11U, planner.state()
                 .latchedWatchdogFault
                 ->lastObservedSequenceHighWatermarkBeforeFault);
}

// Owner-Review R2 (Plan-19.2 #4): ein akkumuliertes Mindestimpulsfenster, das
// die Schwelle erreicht (minimumPulseFromAccumulator), aber am Arming-Gate
// (Minimum-Off) gesperrt ist, wird als DeferredOrLimited verworfen; der
// bereits ins Fenster gebuchte Anteil wird nicht ins Folgefenster
// zurueckgegeben (kein Refund bei Sperre). Fuer beide Richtungen geprueft.
void test_accumulated_minimum_pulse_blocked_at_arming_gate_is_deferred_not_replayed() {
    auto parameters = testParameters();
    parameters.switchingWindowMillis = 1'000U;
    parameters.minimumOnMillis = 100U;
    parameters.minimumOffMillis = 2'000U;
    parameters.polarityDeadTimeMillis = 2'000U;
    parameters.pulseAccumulatorCapMillis = 1'000U;

    for (const auto direction : {AbstractControlDirection::Heating,
                                 AbstractControlDirection::Cooling}) {
        ActuatorPlanner planner{parameters};

        // Voller Puls, physisch aktiv seit t=0.
        static_cast<void>(planner.tick(
            tickInput(0U, demandResult(direction, 1.0, 1U, 0U, airContext()),
                      airContext())));
        // t=150: Teardown nach erfuellter Mindest-On-Zeit -> physischer
        // Deaktivierungsanker bei 150.
        static_cast<void>(planner.tick(
            tickInput(150U, offResult(2U, 150U, airContext()), airContext())));
        TEST_ASSERT_TRUE(
            planner.state()
                .lastPhysicalDeactivationAtMonotonicMillis.has_value());
        TEST_ASSERT_EQUAL_UINT64(
            150U, *planner.state().lastPhysicalDeactivationAtMonotonicMillis);

        // t=200: neue, gleichgerichtete Kleinstquote (50ms je Fenster, unter
        // minimumOnMillis) - erstes Fenster nur Teilgutschrift, noch keine
        // Schwelle.
        auto result = planner.tick(tickInput(
            200U, demandResult(direction, 0.05, 3U, 200U, airContext()),
            airContext()));
        TEST_ASSERT_TRUE(result.reason ==
                         ActuatorPlanReason::AccumulatingBelowThreshold);
        TEST_ASSERT_TRUE(result.appliedDirection ==
                         AbstractControlDirection::Idle);

        // t=1200: Fensterrollover derselben Request - die zweite Gutschrift
        // erreicht exakt minimumOnMillis (50+50=100), aber Minimum-Off ab
        // t=150 ist bei t=1200 (1050ms) noch nicht erfuellt (2000ms noetig)
        // -> DeferredOrLimited statt physischer Aktivierung.
        result = planner.tick(tickInput(1'200U, std::nullopt, airContext()));
        TEST_ASSERT_TRUE(result.reason ==
                         ActuatorPlanReason::MinimumOffTimeHeld);
        TEST_ASSERT_TRUE(result.appliedDirection ==
                         AbstractControlDirection::Idle);
        TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
        TEST_ASSERT_TRUE(
            planner.state().activeWindow->minimumPulseFromAccumulator);
        TEST_ASSERT_EQUAL_UINT64(
            100U, planner.state().activeWindow->scheduledOnMillis);
        assertFeedbackUpdate(
            planner, 3U,
            PreviousControlRequestFeedback::Disposition::DeferredOrLimited);
        // Die verbrauchte Mindestimpuls-Gutschrift wird nicht zurueckgebucht;
        // ein blockiertes Fenster erhaelt keinen erneuten Versuch derselben
        // Gutschrift.
        TEST_ASSERT_EQUAL_DOUBLE(0.0,
                                 planner.state().accumulator.accumulatedMillis);
    }
}

void test_fan_deadlines_and_physical_edges_are_overflow_safe() {
    auto parameters = testParameters();
    parameters.switchingWindowMillis = 1'000U;
    parameters.minimumOnMillis = 100U;
    parameters.minimumOffMillis = 100U;
    parameters.polarityDeadTimeMillis = 100U;
    parameters.pulseAccumulatorCapMillis = 1'000U;
    parameters.outerFanPostRunMillis = 500U;
    const std::uint64_t base =
        std::numeric_limits<std::uint64_t>::max() - 2'000U;
    ActuatorPlanner planner{parameters};
    auto result =
        planner.tick(tickInput(base,
                               demandResult(AbstractControlDirection::Heating,
                                            0.1, 1U, base, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    result = planner.tick(tickInput(base + 100U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
    TEST_ASSERT_TRUE(result.outerFanEnabled);
    TEST_ASSERT_TRUE(
        planner.state().lastPhysicalDeactivationAtMonotonicMillis.has_value());
    result = planner.tick(tickInput(base + 600U, std::nullopt, airContext()));
    TEST_ASSERT_FALSE(result.outerFanEnabled);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
}

void test_air_limit_blocked_off_interrupts_counter_confirmation_during_minimum_on() {
    ActuatorPlanner planner{testParameters()};

    static_cast<void>(planner.tick(
        tickInput(0U,
                  demandResult(AbstractControlDirection::Heating, 1.0, 1U, 0U,
                               productContext(), TemperatureControlReason::None,
                               AirLimitState::Unrestricted),
                  productContext())));
    static_cast<void>(planner.tick(
        tickInput(500U,
                  demandResult(AbstractControlDirection::Cooling, 0.8, 2U, 500U,
                               productContext(), TemperatureControlReason::None,
                               AirLimitState::Unrestricted),
                  productContext())));

    const auto result = planner.tick(
        tickInput(1'000U,
                  offResult(3U, 1'000U, productContext(),
                            TemperatureControlReason::AirLimitBlocked,
                            AirLimitState::Blocked),
                  productContext()));
    TEST_ASSERT_TRUE(result.status == ActuatorPlanStatus::Active);
    TEST_ASSERT_TRUE(result.reason == ActuatorPlanReason::MinimumOnTimeHeld);
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_FALSE(planner.state().counterDirectionCandidate.has_value());
    TEST_ASSERT_TRUE(
        planner.state().counterDirectionObservedSinceMonotonicMillis == 0U);
    TEST_ASSERT_FALSE(planner.state().counterDirectionConfirmed);
    TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
}

void test_fans_follow_physical_output_and_independent_inner_phase() {
    auto parameters = testParameters();
    parameters.innerFanPostRunMillis = 500U;
    ActuatorPlanner planner{parameters};

    auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.3, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_TRUE(result.outerFanEnabled);
    TEST_ASSERT_TRUE(result.innerFanEnabled);

    // The physical pulse ends at 3000 ms, while the planning window remains
    // alive. The mandatory outer post-run is anchored at that real edge.
    result = planner.tick(tickInput(3'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
    TEST_ASSERT_TRUE(result.outerFanEnabled);
    TEST_ASSERT_TRUE(result.innerFanEnabled);
    result = planner.tick(tickInput(4'000U, std::nullopt, airContext()));
    TEST_ASSERT_FALSE(result.outerFanEnabled);
    TEST_ASSERT_TRUE(result.innerFanEnabled);

    // Leaving the temperature-controlled phase starts only the inner-fan
    // post-run; it is independent of the Peltier window.
    result = planner.tick(ActuatorPlanTickInput{
        4'100U, std::nullopt, airContext(), false,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed}});
    TEST_ASSERT_TRUE(result.innerFanEnabled);
    result = planner.tick(ActuatorPlanTickInput{
        4'600U, std::nullopt, airContext(), false,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed}});
    TEST_ASSERT_FALSE(result.innerFanEnabled);
}

void test_feedback_handoff_is_single_use_and_severity_is_monotone() {
    ActuatorPlanner planner{testParameters()};

    auto result =
        planner.tick(tickInput(0U,
                               demandResult(AbstractControlDirection::Heating,
                                            0.1, 1U, 0U, airContext()),
                               airContext()));
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::AccumulatingBelowThreshold);
    auto update = planner.takeFeedbackUpdate();
    TEST_ASSERT_TRUE(update.changed);
    TEST_ASSERT_TRUE(update.feedback.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, update.feedback->controlRequestSequence);
    TEST_ASSERT_TRUE(
        update.feedback->disposition ==
        PreviousControlRequestFeedback::Disposition::DeferredOrLimited);
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);

    // The next window reaches the minimum pulse, but the already observed
    // deferred disposition must not be downgraded to NoIntegratorConstraint.
    result = planner.tick(tickInput(10'000U, std::nullopt, airContext()));
    TEST_ASSERT_TRUE(result.reason ==
                     ActuatorPlanReason::MinimumPulseTriggered);
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);

    // A terminal stop still raises the same sequence to Rejected exactly once.
    result = planner.forceStop(
        11'000U, ActuatorFeedbackEpisodeAtStop::ExistingEpisodeOpen);
    TEST_ASSERT_TRUE(result.appliedDirection == AbstractControlDirection::Idle);
    update = planner.takeFeedbackUpdate();
    TEST_ASSERT_TRUE(update.changed);
    TEST_ASSERT_TRUE(update.feedback.has_value());
    TEST_ASSERT_TRUE(update.feedback->disposition ==
                     PreviousControlRequestFeedback::Disposition::Rejected);
    TEST_ASSERT_FALSE(planner.takeFeedbackUpdate().changed);
}

}  // namespace

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_product_demand_with_unavailable_airlimitstate_is_malformed);
    RUN_TEST(test_air_demand_with_air_limit_reduced_reason_is_malformed);
    RUN_TEST(test_valid_air_and_product_combinations_remain_acceptable);
    RUN_TEST(
        test_confirmed_counter_direction_waits_for_arming_before_new_window);
    RUN_TEST(
        test_below_threshold_counter_request_never_gets_a_window_across_old_window_end);
    RUN_TEST(
        test_normal_off_portion_of_a_started_pulse_stays_scheduled_never_missed);
    RUN_TEST(test_genuinely_late_first_tick_reports_window_pulse_missed);
    RUN_TEST(
        test_confirmed_counter_direction_arming_gate_is_symmetric_for_cooling);
    RUN_TEST(
        test_confirmed_counter_direction_discards_idle_old_window_heating_to_cooling);
    RUN_TEST(
        test_confirmed_counter_direction_discards_idle_old_window_cooling_to_heating);
    RUN_TEST(
        test_neutral_off_interrupts_counter_confirmation_during_minimum_on);
    RUN_TEST(
        test_air_limit_blocked_off_interrupts_counter_confirmation_during_minimum_on);
    RUN_TEST(test_parameter_classification_covers_all_structural_relations);
    RUN_TEST(test_window_ownership_and_variant_b_feedback_are_separate);
    RUN_TEST(
        test_fail_closed_matrix_stops_physical_output_and_trusts_only_sequences);
    RUN_TEST(test_i2a_unconfigured_parameters_reject_immediately);
    RUN_TEST(test_i2b_invalid_parameters_reject_immediately);
    RUN_TEST(test_no_valid_request_and_off_close_planner_without_old_feedback);
    RUN_TEST(test_feedback_disposition_maps_reasons_and_never_downgrades);
    RUN_TEST(test_malformed_evaluation_clears_pending_and_allows_next_sequence);
    RUN_TEST(test_feedback_episode_close_prevents_force_stop_resurrection);
    RUN_TEST(
        test_stale_on_arrival_watchdog_b_closes_episode_a_never_resurrects);
    RUN_TEST(test_stale_on_arrival_context_b_closes_episode_a_never_resurrects);
    RUN_TEST(test_new_stale_off_request_closes_episode_without_any_a_feedback);
    RUN_TEST(
        test_duplicate_evaluation_does_not_disturb_pending_feedback_or_resurrect_a);
    RUN_TEST(test_i_event_after_stale_b_never_resurrects_a_immediate_stop);
    RUN_TEST(test_i_event_after_stale_b_never_resurrects_a_running_watchdog);
    RUN_TEST(
        test_repeated_i4_tick_after_watchdog_trip_does_not_erase_pending_rejected);
    RUN_TEST(
        test_force_stop_existing_episode_open_uses_current_subject_not_stale_accepted_command);
    RUN_TEST(
        test_force_stop_closed_by_outstanding_evaluation_never_emits_old_feedback);
    RUN_TEST(test_watchdog_heartbeat_updates_only_for_new_valid_evaluations);
    RUN_TEST(test_watchdog_episode_anchor_bootstraps_and_h_rebases_deadline);
    RUN_TEST(
        test_forcestop_rebase_after_long_standby_does_not_trip_and_preserves_latch);
    RUN_TEST(
        test_watchdog_fault_evidence_reports_high_watermark_not_last_accepted);
    RUN_TEST(
        test_accumulated_minimum_pulse_blocked_at_arming_gate_is_deferred_not_replayed);
    RUN_TEST(test_fan_deadlines_and_physical_edges_are_overflow_safe);
    RUN_TEST(test_fans_follow_physical_output_and_independent_inner_phase);
    RUN_TEST(test_feedback_handoff_is_single_use_and_severity_is_monotone);
    return UNITY_END();
}
