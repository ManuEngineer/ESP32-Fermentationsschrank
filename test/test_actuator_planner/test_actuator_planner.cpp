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
    return UNITY_END();
}
