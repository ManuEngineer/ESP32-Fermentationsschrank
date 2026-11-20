#include <unity.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "configuration_service.hpp"
#include "control_chain.hpp"
#include "control_sensor_evidence.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "mock_bidirectional_actuator_sink.hpp"
#include "mock_binary_output_sink.hpp"
#include "mock_time_zone_resolver.hpp"
#include "run_persistence_coordinator.hpp"
#include "sensor_quality_config.hpp"
#include "simulated_persistent_state_store.hpp"
#include "standard_program_catalog.hpp"
#include "temperature_source.hpp"
#include "virtual_time_source.hpp"

// Issue #218 (SIM-218-*): the R1 control chain exercised through the real
// FermentationApplication, the real ControlSensorEvidenceProducer and the
// real ActuationInterlock evidence builder. All numeric values below are
// explicit TEST fixtures; none of them is a product value (#35).

namespace fermentation {

class RunPersistenceCoordinatorTestAccess {
   public:
    static void setState(RunPersistenceCoordinator& coordinator,
                         RunPersistenceCoordinatorState state) {
        coordinator.state_ = state;
    }
};

class FermentationApplicationTestAccess {
   public:
    static FermentationApplicationRequestResult prepareStartManualTimedBody(
        FermentationApplication& application,
        const FermentationUiCommandContext& context,
        const ManualTimedRunValues& values) {
        return application.prepareStartManualTimedUnguarded(context, values);
    }
    static ConfigurationService& configurationService(
        FermentationApplication& application) {
        return *application.configurationService_;
    }
    static RunPersistenceCoordinator& coordinator(
        FermentationApplication& application) {
        return *application.runPersistenceCoordinator_;
    }
    static RunCommandState& runtimeState(FermentationApplication& application) {
        return *application.runtimeRunState_;
    }
    static bool activationLatched(const FermentationApplication& application) {
        return application.freshStartActivation_.active;
    }
    static bool chainPresent(const FermentationApplication& application) {
        return application.controlChain_ != nullptr;
    }
    static const ActuatorPlanner& planner(
        const FermentationApplication& application) {
        return application.controlChain_->planner;
    }
    static void setLoadStatus(FermentationApplication& application,
                              RunPersistenceLoadStatus status) {
        application.persistenceLoadStatus_ = status;
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;
using device_platform_test_support::MockBidirectionalActuatorSink;
using device_platform_test_support::MockBinaryOutputSink;

class ScriptedSource final : public device_platform::ITemperatureSource {
   public:
    explicit ScriptedSource(const device_platform::VirtualTimeSource& time)
        : time_(time) {}
    void setCelsius(double celsius) {
        celsius_ = celsius;
        failed_ = false;
    }
    void setFailed() { failed_ = true; }
    [[nodiscard]] device_platform::TemperatureReading read() const override {
        const auto now = time_.monotonicMillis();
        if (failed_) {
            return device_platform::TemperatureReading::create(
                       std::nullopt, now,
                       device_platform::TemperatureSampleStatus::BusFault,
                       std::nullopt)
                .reading.value();
        }
        return device_platform::TemperatureReading::create(
                   std::nullopt, now,
                   device_platform::TemperatureSampleStatus::Ok, celsius_)
            .reading.value();
    }

   private:
    const device_platform::VirtualTimeSource& time_;
    double celsius_{20.0};
    bool failed_{false};
};

// TEST values only.
ControlChainParameters testChainParameters() {
    ControlChainParameters result;
    result.pi.airHeating = {0.10, 0.01, 1.0, 0.8};
    result.pi.airCooling = {0.10, 0.01, 1.0, 0.8};
    result.pi.productHeating = {0.20, 0.02, 1.0, 0.8};
    result.pi.productCooling = {0.20, 0.02, 1.0, 0.8};
    result.pi.maximumIntegrationStepMillis = 10'000U;
    result.pi.airLimitLowerBlockCelsius = 5.0;
    result.pi.airLimitLowerReduceStartCelsius = 10.0;
    result.pi.airLimitUpperReduceStartCelsius = 45.0;
    result.pi.airLimitUpperBlockCelsius = 50.0;
    result.policy = {IntegratorTransitionAction::Reset,
                     IntegratorTransitionAction::Reset,
                     IntegratorTransitionAction::Reset, 0.2};
    return result;
}

ActuatorPlannerParameters testPlannerParameters() {
    ActuatorPlannerParameters parameters;
    parameters.switchingWindowMillis = 10'000U;
    parameters.minimumOnMillis = 2'000U;
    parameters.minimumOffMillis = 1'000U;
    parameters.polarityDeadTimeMillis = 3'000U;
    parameters.pulseAccumulatorCapMillis = 10'000U;
    parameters.counterDirectionConfirmationQuoteThreshold = 0.5;
    parameters.counterDirectionConfirmationDurationMillis = 2'000U;
    parameters.requestWatchdogMillis = 60'000U;
    parameters.outerFanPostRunMillis = 3'000U;
    parameters.innerFanPostRunMillis = 1'000U;
    return parameters;
}

// TEST values only (wide on purpose): the firmware temperature safety must be
// configured for any Peltier permission (R1 trial plan S6).
ThermalSafetyLimits testThermalLimits() {
    ThermalSafetyLimits limits;
    limits.airEmergencyLowerCelsius = -30.0;
    limits.airInterventionLowerCelsius = -20.0;
    limits.airInterventionUpperCelsius = 60.0;
    limits.airEmergencyUpperCelsius = 70.0;
    limits.coolingEmergencyLowerCelsius = -30.0;
    limits.coolingInterventionLowerCelsius = -20.0;
    limits.coolingInterventionUpperCelsius = 80.0;
    limits.coolingEmergencyUpperCelsius = 90.0;
    limits.coolingMaxRiseCelsiusPerMinute = 50.0;
    // Shortest permitted trend window: Ok needs half of it observed (5 s).
    limits.trendWindowMillis = 10'000U;
    limits.maxSampleAgeMillis = 5'000U;
    limits.invalidLatchMillis = 20'000U;
    return limits;
}

device_platform::SensorQualityConfig testQualityConfig() {
    return device_platform::SensorQualityConfig::create(
               3U, 5.0, -20.0, 80.0, 100.0, 10'000U, 3U, 2U, 2'000U)
        .config.value();
}

struct Chain {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource time;
    MockBidirectionalActuatorSink peltier;
    MockBinaryOutputSink outerFan;
    MockBinaryOutputSink innerFan;
    ScriptedSource air{time};
    ScriptedSource cooling{time};
    ScriptedSource product{time};
    std::optional<ControlSensorEvidenceProducer> producer;
    FermentationApplication application;
    bool publishEvidence{true};

    explicit Chain(const ControlChainParameters& parameters,
                   bool withQualityConfig = true) {
        time.setUnixTimeSeconds(1'700'000'000LL);
        if (withQualityConfig) {
            producer.emplace(
                air, cooling, product, time,
                std::optional<device_platform::SensorQualityConfig>{
                    testQualityConfig()});
        } else {
            producer.emplace(
                air, cooling, product, time,
                std::optional<device_platform::SensorQualityConfig>{});
        }
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.attachActuationPorts(
            ActuatorPlanSinkPorts{&peltier, &outerFan, &innerFan}, parameters));
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, time));
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::chainPresent(application));
    }

    void installThermalSafety() {
        TEST_ASSERT_TRUE(
            application.configureThermalSafety(testThermalLimits()) ==
            ThermalSafetyConfigureResult::Accepted);
    }

    void installPlannerParameters(bool withThermalSafety = true) {
        if (withThermalSafety) installThermalSafety();
        auto& service = FermentationApplicationTestAccess::configurationService(
            application);
        auto build = service.beginPreview();
        TEST_ASSERT_TRUE(build.status == ConfigurationPreviewStatus::Success);
        build.lease.serviceConfiguration().actuatorPlannerParameters =
            testPlannerParameters();
        auto installed = service.installPreview(std::move(build.lease),
                                                decodeChangeOrigin(2U),
                                                decodeChangeOperation(1U));
        TEST_ASSERT_TRUE(installed.status ==
                         ConfigurationPreviewStatus::Success);
        TEST_ASSERT_TRUE(
            service.confirmPreview(installed.preview->handle).status ==
            ConfigurationCommitStatus::Activated);
    }

    // One application loop iteration: advance time, produce and publish the
    // per-role evidence (as app_main does), then update().
    void step(std::uint64_t deltaMs = 1'000U) {
        time.advanceMonotonicMillis(deltaMs);
        application.publishRawTemperatureSamples(producer->readRawSamples());
        if (publishEvidence) {
            const auto phase = application.publishedProcessState()
                                   .value_or(ProcessRuntimeState{})
                                   .state;
            if (const auto evidence = producer->sample(phase)) {
                application.publishOwningRuntimeEvidence(*evidence);
            }
        }
        application.update();
    }

    void warmUpSensors(int ticks = 8) {
        for (int i = 0; i < ticks; ++i) step();
    }

    // Same as startManualRun() but reports a refused start instead of failing
    // (R1 trial plan S2: no start without regulation readiness).
    [[nodiscard]] bool tryStartManualRun() {
        ManualTimedRunValues values;
        values.targetTemperatureCelsius = 30.0;
        values.durationMinutes = 60U;
        values.qualificationBandCelsius = 0.5;
        values.qualificationDurationMinutes = 10U;
        values.maximumTargetReachMinutes = 180U;
        FermentationUiCommandContext context;
        context.expected = application.uiSnapshot().revisions;
        context.monotonicMillis = time.monotonicMillis();
        const auto prepared =
            FermentationApplicationTestAccess::prepareStartManualTimedBody(
                application, context, values);
        if (!prepared.request.has_value()) return false;
        const auto confirmed = application.confirmPrepared(prepared);
        if (!confirmed.request.has_value()) return false;
        static_cast<void>(
            application.applyConfirmedPrepared(*confirmed.request));
        return application.uiSnapshot().home.mode ==
               FermentationHomeMode::ActiveRun;
    }

    void startManualRun() {
        ManualTimedRunValues values;
        values.targetTemperatureCelsius = 30.0;
        values.durationMinutes = 60U;
        values.qualificationBandCelsius = 0.5;
        values.qualificationDurationMinutes = 10U;
        values.maximumTargetReachMinutes = 180U;
        FermentationUiCommandContext context;
        context.expected = application.uiSnapshot().revisions;
        context.monotonicMillis = time.monotonicMillis();
        const auto prepared =
            FermentationApplicationTestAccess::prepareStartManualTimedBody(
                application, context, values);
        TEST_ASSERT_TRUE(prepared.request.has_value());
        const auto confirmed = application.confirmPrepared(prepared);
        TEST_ASSERT_TRUE(confirmed.request.has_value());
        const auto applied =
            application.applyConfirmedPrepared(*confirmed.request);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
            static_cast<int>(applied.phase));
        TEST_ASSERT_TRUE(application.uiSnapshot().home.mode ==
                         FermentationHomeMode::ActiveRun);
    }

    void stopRun() {
        FermentationUiCommandContext context;
        context.expected = application.uiSnapshot().revisions;
        context.monotonicMillis = time.monotonicMillis();
        FermentationUiStopRunIntent stop;
        stop.option = StopOption::AbortAndTurnOff;
        const auto prepared = application.prepareStop(context, stop);
        TEST_ASSERT_TRUE(prepared.request.has_value());
        const auto confirmed = application.confirmPrepared(prepared);
        TEST_ASSERT_TRUE(confirmed.request.has_value());
        static_cast<void>(
            application.applyConfirmedPrepared(*confirmed.request));
        TEST_ASSERT_TRUE(application.uiSnapshot().home.mode ==
                         FermentationHomeMode::Standby);
    }

    // Invariant of every single step: the bridge never has both directions on
    // and a direction is only on while the outer fan is on.
    void assertSafeOutputs() const {
        TEST_ASSERT_FALSE(peltier.forward() && peltier.reverse());
        TEST_ASSERT_FALSE(peltier.simultaneousActivationObserved());
        if (peltier.forward() || peltier.reverse()) {
            TEST_ASSERT_TRUE(outerFan.enabled());
        }
    }

    [[nodiscard]] bool peltierOn() const {
        return peltier.forward() || peltier.reverse();
    }

    // Runs until the Peltier is on (heating demand); returns the ticks used.
    int runUntilPeltierOn(int maxTicks = 60) {
        for (int i = 1; i <= maxTicks; ++i) {
            step();
            assertSafeOutputs();
            if (peltierOn()) return i;
        }
        return -1;
    }

    void establishHeating() {
        installPlannerParameters();
        air.setCelsius(20.0);
        cooling.setCelsius(20.0);
        product.setCelsius(20.0);
        warmUpSensors();
        startManualRun();
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::activationLatched(application));
        TEST_ASSERT_TRUE(runUntilPeltierOn() > 0);
    }

    void assertAllOff() const {
        TEST_ASSERT_FALSE_MESSAGE(peltier.forward(), "peltier forward");
        TEST_ASSERT_FALSE_MESSAGE(peltier.reverse(), "peltier reverse");
        TEST_ASSERT_FALSE_MESSAGE(outerFan.enabled(), "outer fan");
        TEST_ASSERT_FALSE_MESSAGE(innerFan.enabled(), "inner fan");
    }
};

// T1: explicit start + fresh required sensors -> regulator -> planner ->
// outer fan ON before exactly one bridge direction; two or more consecutive
// positive ticks of the same committed run; STOP ends it immediately.
void test_sim_218_01_positive_chain_and_stop() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    TEST_ASSERT_TRUE(chain.outerFan.enabled());
    TEST_ASSERT_TRUE(chain.peltier.forward());
    TEST_ASSERT_FALSE(chain.peltier.reverse());

    // At least two consecutive ticks with the same committed run keep the
    // chain active and the planner accepted two distinct evaluations.
    const auto firstSequence =
        FermentationApplicationTestAccess::planner(chain.application)
            .state()
            .lastObservedSequenceHighWatermark;
    TEST_ASSERT_TRUE(firstSequence.has_value());
    chain.step();
    chain.assertSafeOutputs();
    chain.step();
    chain.assertSafeOutputs();
    const auto laterSequence =
        FermentationApplicationTestAccess::planner(chain.application)
            .state()
            .lastObservedSequenceHighWatermark;
    TEST_ASSERT_TRUE(laterSequence.has_value());
    TEST_ASSERT_TRUE(*laterSequence > *firstSequence);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::activationLatched(
        chain.application));

    // STOP: the Peltier is off inside the command itself, the activation is
    // gone in the same call, the permitted fan run-on follows the planner and
    // ends; nothing re-enables the Peltier.
    chain.stopRun();
    TEST_ASSERT_FALSE(chain.peltierOn());
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::activationLatched(
        chain.application));
    for (int i = 0; i < 12; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
        chain.assertSafeOutputs();
    }
    chain.assertAllOff();
}

// T2: required sensor missing/failed (cooling body) -> Peltier off and locked.
void test_sim_218_02_cooling_sensor_failure_locks_peltier() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    chain.cooling.setFailed();
    bool off = false;
    for (int i = 0; i < 20 && !off; ++i) {
        chain.step();
        chain.assertSafeOutputs();
        off = !chain.peltierOn();
    }
    TEST_ASSERT_TRUE(off);
    for (int i = 0; i < 10; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
    }
}

// T2: stale evidence (nothing published any more) -> immediately locked.
void test_sim_218_03_stale_evidence_locks_peltier() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    chain.publishEvidence = false;
    chain.step();
    TEST_ASSERT_FALSE(chain.peltierOn());
    for (int i = 0; i < 5; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
    }
}

// T2: outer fan ON refused -> the Peltier is never released.
void test_sim_218_04_refused_outer_fan_blocks_peltier() {
    Chain chain(testChainParameters());
    chain.outerFan.setAcceptingCommands(false);
    chain.installPlannerParameters();
    chain.warmUpSensors();
    chain.startManualRun();
    for (int i = 0; i < 40; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
        TEST_ASSERT_FALSE(chain.peltier.simultaneousActivationObserved());
    }
    TEST_ASSERT_TRUE(chain.outerFan.rejectedCommandCount() > 0U);
}

// T2: unsafe persistence state -> locked (load status and coordinator state).
void test_sim_218_05_unsafe_persistence_locks_peltier() {
    {
        Chain chain(testChainParameters());
        chain.establishHeating();
        FermentationApplicationTestAccess::setLoadStatus(
            chain.application, RunPersistenceLoadStatus::ReadFailed);
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
    }
    {
        Chain chain(testChainParameters());
        chain.establishHeating();
        RunPersistenceCoordinatorTestAccess::setState(
            FermentationApplicationTestAccess::coordinator(chain.application),
            RunPersistenceCoordinatorState::BlockedIndeterminate);
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
    }
}

// T2: a leaving of the temperature-controlled phase (fault transition) ends
// the activation at the next tick even without a handoff (per-tick re-check).
void test_sim_218_06_fault_phase_locks_peltier() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    auto& state =
        FermentationApplicationTestAccess::runtimeState(chain.application);
    const auto fault = decideProcessTransition(
        state.processState, &*state.processRunSnapshot,
        ProcessSignals{QualificationProgress::Unavailable, false, true},
        TransitionRequest{}, chain.time.monotonicMillis());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            FermentationApplicationTestAccess::coordinator(chain.application)
                .persistTransition(
                    state, fault,
                    RunCheckpointTime{chain.time.monotonicMillis(),
                                      1'700'000'000LL})
                .status));
    chain.step();
    TEST_ASSERT_FALSE(chain.peltierOn());
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::activationLatched(
        chain.application));
    for (int i = 0; i < 5; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
    }
}

// T2/T3: restart (begin) while heating with an old Applied start: final
// teardown turns everything off, and the persisted run never re-activates.
void test_sim_218_07_rebegin_tears_down_and_never_reactivates() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    TEST_ASSERT_TRUE(chain.outerFan.enabled());
    TEST_ASSERT_TRUE(chain.application.begin(
        chain.platform, chain.store, chain.timeZoneResolver, chain.time));
    chain.assertAllOff();
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::activationLatched(
        chain.application));
    for (int i = 0; i < 10; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
    }
}

// T3: teardown while a fan run-on is still open turns both fans off (final
// teardown is distinct from the normal run STOP, which keeps the run-on).
void test_sim_218_08_final_teardown_ends_open_fan_run_on() {
    MockBidirectionalActuatorSink* peltierPtr = nullptr;
    Chain chain(testChainParameters());
    peltierPtr = &chain.peltier;
    chain.establishHeating();
    chain.stopRun();
    // Normal STOP keeps the permitted run-on of at least one fan.
    TEST_ASSERT_TRUE(chain.outerFan.enabled() || chain.innerFan.enabled());
    TEST_ASSERT_FALSE(peltierPtr->forward());
    TEST_ASSERT_TRUE(chain.application.begin(
        chain.platform, chain.store, chain.timeZoneResolver, chain.time));
    chain.assertAllOff();
}

// T3: product default. (a)/(b) Unconfigured regulator parameters (with and
// without a bound test planner snapshot) and otherwise fresh evidence: no
// ControlRequest, hence no Peltier and no outer fan. The inner fan follows the
// existing planner rule (ON in any temperature-controlled phase, independent
// of the gate); in the product no run can be started without Valid sensors,
// which need the unconfigured quality configuration (#35). (c) No sensor
// quality configuration: the producer yields no evidence, every sink is off.
void test_sim_218_09_product_default_is_fail_closed() {
    for (const bool plannerSnapshot : {false, true}) {
        Chain chain(kControlChainParametersUnconfiguredUntilIssue35, true);
        if (plannerSnapshot) chain.installPlannerParameters();
        chain.warmUpSensors();
        // The product default (PI parameters unconfigured) never starts a run
        // without effective regulation: the start is refused up front.
        TEST_ASSERT_FALSE(chain.tryStartManualRun());
        for (int i = 0; i < 40; ++i) {
            chain.step();
            TEST_ASSERT_FALSE(chain.peltierOn());
            TEST_ASSERT_FALSE(chain.outerFan.enabled());
        }
    }
    {
        Chain chain(kControlChainParametersUnconfiguredUntilIssue35, false);
        TEST_ASSERT_FALSE(chain.producer->configured());
        chain.installPlannerParameters();
        chain.publishEvidence = false;
        CrossRolePlausibilityContext manual;
        manual.air.quality = device_platform::SensorQuality::Valid;
        manual.cooling.quality = device_platform::SensorQuality::Valid;
        manual.product.quality = device_platform::SensorQuality::Valid;
        for (int i = 0; i < 40; ++i) {
            chain.time.advanceMonotonicMillis(1'000U);
            chain.application.publishOwningRuntimeEvidence(manual);
            chain.application.update();
            chain.assertAllOff();
        }
        TEST_ASSERT_FALSE(
            chain.producer->sample(ProcessState::Standby).has_value());
    }
}

// T3: without attachActuationPorts the application never touches a port;
// ports are rejected after begin().
void test_sim_218_10_no_ports_no_chain_and_late_attach_rejected() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource time;
    time.setUnixTimeSeconds(1'700'000'000LL);
    MockBidirectionalActuatorSink peltier;
    MockBinaryOutputSink outerFan;
    MockBinaryOutputSink innerFan;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(
        application.begin(platform, store, timeZoneResolver, time));
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::chainPresent(application));
    TEST_ASSERT_FALSE(application.attachActuationPorts(
        ActuatorPlanSinkPorts{&peltier, &outerFan, &innerFan},
        testChainParameters()));
    for (int i = 0; i < 5; ++i) application.update();
    TEST_ASSERT_TRUE(peltier.commandJournal().empty());
    TEST_ASSERT_TRUE(outerFan.commandJournal().empty());
    TEST_ASSERT_TRUE(innerFan.commandJournal().empty());
}

}  // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------
// R1 trial plan S6/S2: firmware temperature safety and regulation readiness.
// ---------------------------------------------------------------------------

// A cooling-body excursion trips the supervisor: Peltier off at once, a visible
// latched fault, the permitted fan run-on is not cut, and nothing re-arms
// without a restart.
void test_s6_trip_locks_peltier_is_visible_and_latched() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    TEST_ASSERT_TRUE(chain.peltierOn());
    chain.cooling.setCelsius(85.0);  // above the cooling intervention limit
    chain.step();
    TEST_ASSERT_FALSE(chain.peltierOn());
    chain.assertSafeOutputs();
    const auto evaluation = chain.application.thermalSafetyEvaluation();
    TEST_ASSERT_TRUE(evaluation.status == ThermalSafetyStatus::Tripped);
    TEST_ASSERT_TRUE(evaluation.reason ==
                     ThermalSafetyReason::CoolingUpperIntervention);
    TEST_ASSERT_TRUE(chain.application.lifecycleState() ==
                     ApplicationLifecycleState::ServiceRequired);
    TEST_ASSERT_TRUE(chain.application.presentationState().faultCode ==
                     FaultCode::TemperatureSafetyLimit);
    // Values return to normal: the latch holds, nothing re-enables the Peltier
    // and the configuration is refused until a restart.
    chain.cooling.setCelsius(30.0);
    for (int i = 0; i < 40; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
        chain.assertSafeOutputs();
    }
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                     ThermalSafetyStatus::Tripped);
    // Re-configuration is refused while the run exists (and the latch holds).
    TEST_ASSERT_TRUE(
        chain.application.configureThermalSafety(testThermalLimits()) ==
        ThermalSafetyConfigureResult::RunActiveOrUnknown);
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                     ThermalSafetyStatus::Tripped);
    // The fan run-on of the planner ended on its own schedule (not stuck on).
    TEST_ASSERT_FALSE(chain.outerFan.enabled());
}

void test_s6_trip_keeps_the_planner_outer_fan_run_on() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    TEST_ASSERT_TRUE(chain.outerFan.enabled());
    chain.cooling.setCelsius(85.0);
    chain.step();
    TEST_ASSERT_FALSE(chain.peltierOn());
    // The outer fan is switched off only after the configured run-on (3 s in
    // this fixture) - never earlier than the Peltier, never left on forever.
    bool fanWasOnAfterPeltierOff = chain.outerFan.enabled();
    for (int i = 0; i < 12; ++i) {
        chain.step(500U);
        fanWasOnAfterPeltierOff =
            fanWasOnAfterPeltierOff || chain.outerFan.enabled();
        chain.assertSafeOutputs();
    }
    TEST_ASSERT_TRUE(fanWasOnAfterPeltierOff);
    TEST_ASSERT_FALSE(chain.outerFan.enabled());
}

// Stale raw samples block (no latch); fresh samples bring it back to Ok.
void test_s6_stale_raw_samples_block_without_latching() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    chain.time.advanceMonotonicMillis(6'000U);  // no new raw publication
    chain.application.update();
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                     ThermalSafetyStatus::Blocked);
    TEST_ASSERT_FALSE(chain.peltierOn());
    chain.step();  // fresh raw samples again after a gap: observation restarts
    auto evaluation = chain.application.thermalSafetyEvaluation();
    TEST_ASSERT_TRUE(evaluation.status == ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(evaluation.reason ==
                     ThermalSafetyReason::TrendObservationPending);
    TEST_ASSERT_FALSE(chain.peltierOn());
    for (int i = 0; i < 6; ++i) chain.step();  // half of the 10 s window
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                     ThermalSafetyStatus::Ok);
    TEST_ASSERT_TRUE(chain.application.lifecycleState() ==
                     ApplicationLifecycleState::Ready);
}

// Without any limits the supervisor is Unconfigured: even complete other
// evidence never releases the Peltier.
void test_s6_unconfigured_thermal_safety_never_releases_the_peltier() {
    Chain chain(testChainParameters());
    chain.installPlannerParameters(false);
    chain.air.setCelsius(20.0);
    chain.cooling.setCelsius(20.0);
    chain.product.setCelsius(20.0);
    chain.warmUpSensors();
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                     ThermalSafetyStatus::Unconfigured);
    TEST_ASSERT_FALSE(chain.tryStartManualRun());
    for (int i = 0; i < 20; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
    }
}

// R221-2: the protection is never reconfigured while a run exists. A refused
// call changes neither the limits nor the state, the latch or the running
// invalid-episode timer.
void test_s6_configuration_is_refused_during_an_active_run() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    TEST_ASSERT_TRUE(chain.application.uiSnapshot().home.mode ==
                     FermentationHomeMode::ActiveRun);
    const auto limitsBefore = chain.application.thermalSafetyLimits();
    auto wider = testThermalLimits();
    wider.coolingInterventionUpperCelsius = 85.0;  // would widen the limit
    wider.coolingEmergencyUpperCelsius = 95.0;
    TEST_ASSERT_TRUE(chain.application.configureThermalSafety(wider) ==
                     ThermalSafetyConfigureResult::RunActiveOrUnknown);
    TEST_ASSERT_TRUE(
        chain.application.configureThermalSafety(testThermalLimits()) ==
        ThermalSafetyConfigureResult::RunActiveOrUnknown);
    const auto limitsAfter = chain.application.thermalSafetyLimits();
    TEST_ASSERT_TRUE(limitsAfter.coolingInterventionUpperCelsius ==
                     limitsBefore.coolingInterventionUpperCelsius);
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                     ThermalSafetyStatus::Ok);
    TEST_ASSERT_TRUE(chain.application.uiSnapshot().home.mode ==
                     FermentationHomeMode::ActiveRun);
    // The original (narrower) limit is still in force: a value between the old
    // and the refused wider limit trips.
    chain.cooling.setCelsius(82.0);
    chain.step();
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                     ThermalSafetyStatus::Tripped);
    TEST_ASSERT_FALSE(chain.peltierOn());
}

// A running invalid-reading episode is not reset by a refused configuration:
// the loss latch still fires on its original schedule.
void test_s6_a_refused_configuration_does_not_reset_the_invalid_episode() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    chain.cooling.setFailed();  // raw BusFault: Blocked, timer starts
    for (int i = 0; i < 15; ++i) {
        chain.step();
        TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                         ThermalSafetyStatus::Blocked);
    }
    TEST_ASSERT_TRUE(
        chain.application.configureThermalSafety(testThermalLimits()) ==
        ThermalSafetyConfigureResult::RunActiveOrUnknown);
    for (int i = 0; i < 4; ++i) chain.step();  // the 20 s latch is near
    for (int i = 0; i < 3; ++i) chain.step();  // passed on the original timer
    const auto evaluation = chain.application.thermalSafetyEvaluation();
    TEST_ASSERT_TRUE(evaluation.status == ThermalSafetyStatus::Tripped);
    TEST_ASSERT_TRUE(evaluation.reason == ThermalSafetyReason::SampleLoss);
    TEST_ASSERT_FALSE(chain.peltierOn());
}

// The trend history survives a refused configuration (a dangerous rise during
// the run still trips on its original observation).
void test_s6_a_refused_configuration_keeps_the_trend_history() {
    auto limits = testThermalLimits();
    limits.coolingMaxRiseCelsiusPerMinute = 6.0;
    Chain chain(testChainParameters());
    chain.installPlannerParameters(false);
    TEST_ASSERT_TRUE(chain.application.configureThermalSafety(limits) ==
                     ThermalSafetyConfigureResult::Accepted);
    chain.air.setCelsius(20.0);
    chain.cooling.setCelsius(20.0);
    chain.product.setCelsius(20.0);
    chain.warmUpSensors();
    TEST_ASSERT_TRUE(chain.tryStartManualRun());
    double cooling = 20.0;
    bool tripped = false;
    for (int i = 0; i < 40 && !tripped; ++i) {
        cooling += 0.5;  // +30 K/min at 1 s steps
        chain.cooling.setCelsius(cooling);
        chain.step();
        if (i == 3) {
            TEST_ASSERT_TRUE(chain.application.configureThermalSafety(limits) ==
                             ThermalSafetyConfigureResult::RunActiveOrUnknown);
        }
        tripped = chain.application.thermalSafetyEvaluation().status ==
                  ThermalSafetyStatus::Tripped;
    }
    TEST_ASSERT_TRUE(tripped);
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().reason ==
                         ThermalSafetyReason::CoolingRiseRate ||
                     chain.application.thermalSafetyEvaluation().reason ==
                         ThermalSafetyReason::CoolingUpperIntervention);
    TEST_ASSERT_FALSE(chain.peltierOn());
}

// `thermal clear` during a run is safety-directed: the Peltier is blocked at
// once, the fan run-on is kept, and nothing restarts silently (a refused
// re-configuration, the run keeps its blocked gate).
void test_s6_clear_during_a_run_blocks_the_peltier_and_cannot_restart_silently() {
    Chain chain(testChainParameters());
    chain.establishHeating();
    TEST_ASSERT_TRUE(chain.peltierOn());
    chain.application.clearThermalSafetyConfiguration();
    // Immediately (no further loop iteration) the Peltier is off.
    TEST_ASSERT_FALSE(chain.peltierOn());
    chain.assertSafeOutputs();
    TEST_ASSERT_TRUE(chain.application.thermalSafetyEvaluation().status ==
                     ThermalSafetyStatus::Unconfigured);
    // The planner keeps its run-on schedule and ends it by itself.
    bool fanOn = chain.outerFan.enabled();
    for (int i = 0; i < 14; ++i) {
        chain.step(500U);
        fanOn = fanOn || chain.outerFan.enabled();
        TEST_ASSERT_FALSE(chain.peltierOn());
        chain.assertSafeOutputs();
    }
    TEST_ASSERT_TRUE(fanOn);
    TEST_ASSERT_FALSE(chain.outerFan.enabled());
    // No silent restart: a re-configuration during the run is refused, the
    // gate stays denied for any amount of time.
    TEST_ASSERT_TRUE(
        chain.application.configureThermalSafety(testThermalLimits()) ==
        ThermalSafetyConfigureResult::RunActiveOrUnknown);
    for (int i = 0; i < 40; ++i) {
        chain.step();
        TEST_ASSERT_FALSE(chain.peltierOn());
    }
    TEST_ASSERT_TRUE(chain.application.uiSnapshot().home.mode ==
                     FermentationHomeMode::ActiveRun);
    // After the run ends, the protection can be configured again, but a new
    // start is needed (and refused until everything is ready).
    chain.stopRun();
    TEST_ASSERT_TRUE(
        chain.application.configureThermalSafety(testThermalLimits()) ==
        ThermalSafetyConfigureResult::Accepted);
    TEST_ASSERT_FALSE(chain.peltierOn());
}

// Not started: the run state is unknown, so the configuration is refused.
void test_s6_configuration_without_a_run_state_is_refused() {
    FermentationApplication application;
    TEST_ASSERT_TRUE(application.configureThermalSafety(testThermalLimits()) ==
                     ThermalSafetyConfigureResult::RunActiveOrUnknown);
    TEST_ASSERT_FALSE(application.thermalSafetyEvaluation().status ==
                      ThermalSafetyStatus::Ok);
}

// S2: the start admission needs regulation readiness at prepare AND confirm.
void test_s2_start_admission_matrix() {
    {  // everything ready -> start possible
        Chain chain(testChainParameters());
        chain.installPlannerParameters();
        chain.warmUpSensors();
        TEST_ASSERT_TRUE(chain.tryStartManualRun());
    }
    {  // no planner parameters
        Chain chain(testChainParameters());
        chain.installThermalSafety();
        chain.warmUpSensors();
        TEST_ASSERT_FALSE(chain.tryStartManualRun());
    }
    {  // PI parameters unconfigured
        Chain chain(kControlChainParametersUnconfiguredUntilIssue35);
        chain.installPlannerParameters();
        chain.warmUpSensors();
        TEST_ASSERT_FALSE(chain.tryStartManualRun());
    }
    {  // thermal safety not configured
        Chain chain(testChainParameters());
        chain.installPlannerParameters(false);
        chain.warmUpSensors();
        TEST_ASSERT_FALSE(chain.tryStartManualRun());
    }
    {  // thermal safety blocked (stale raw samples)
        Chain chain(testChainParameters());
        chain.installPlannerParameters();
        chain.warmUpSensors();
        chain.time.advanceMonotonicMillis(6'000U);
        TEST_ASSERT_FALSE(chain.tryStartManualRun());
    }
    {  // thermal safety tripped (limit excursion before the start)
        Chain chain(testChainParameters());
        chain.installPlannerParameters();
        chain.warmUpSensors();
        chain.cooling.setCelsius(85.0);
        chain.step();
        TEST_ASSERT_FALSE(chain.tryStartManualRun());
    }
}

// A state jump between prepare and confirm refuses the start at confirm.
void test_s2_state_jump_between_prepare_and_confirm_is_refused() {
    for (const int mode : {0, 1, 2}) {
        Chain chain(testChainParameters());
        chain.installPlannerParameters();
        chain.warmUpSensors();
        ManualTimedRunValues values;
        values.targetTemperatureCelsius = 30.0;
        values.durationMinutes = 60U;
        values.qualificationBandCelsius = 0.5;
        values.qualificationDurationMinutes = 10U;
        values.maximumTargetReachMinutes = 180U;
        FermentationUiCommandContext context;
        context.expected = chain.application.uiSnapshot().revisions;
        context.monotonicMillis = chain.time.monotonicMillis();
        const auto prepared =
            FermentationApplicationTestAccess::prepareStartManualTimedBody(
                chain.application, context, values);
        TEST_ASSERT_TRUE(prepared.request.has_value());
        if (mode == 0) {
            chain.cooling.setCelsius(85.0);  // trip
            chain.step();
        } else if (mode == 1) {
            chain.time.advanceMonotonicMillis(6'000U);  // raw samples go stale
        } else {
            chain.air.setFailed();  // raw sample fault -> blocked
            chain.step();
        }
        const auto confirmed = chain.application.confirmPrepared(prepared);
        TEST_ASSERT_FALSE(confirmed.request.has_value());
        TEST_ASSERT_TRUE(chain.application.uiSnapshot().home.mode !=
                         FermentationHomeMode::ActiveRun);
    }
}

// The program start is refused before any identity-effective step.
void test_s2_program_start_is_refused_before_the_command_identity_is_used() {
    Chain chain(kControlChainParametersUnconfiguredUntilIssue35);
    chain.warmUpSensors();
    const auto before =
        FermentationApplicationTestAccess::coordinator(chain.application)
            .commandIdHighWater();
    FermentationUiCommandContext context;
    context.expected = chain.application.uiSnapshot().revisions;
    context.monotonicMillis = chain.time.monotonicMillis();
    FermentationUiStartProgramIntent intent;
    intent.candidate.programId =
        FactoryProgramCatalog::programs().front().program.id;
    const auto result = chain.application.prepareStartProgram(context, intent);
    TEST_ASSERT_TRUE(result.status ==
                     FermentationApplicationRequestStatus::Unavailable);
    TEST_ASSERT_FALSE(result.request.has_value());
    const auto after =
        FermentationApplicationTestAccess::coordinator(chain.application)
            .commandIdHighWater();
    TEST_ASSERT_TRUE(before == after);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sim_218_01_positive_chain_and_stop);
    RUN_TEST(test_sim_218_02_cooling_sensor_failure_locks_peltier);
    RUN_TEST(test_sim_218_03_stale_evidence_locks_peltier);
    RUN_TEST(test_sim_218_04_refused_outer_fan_blocks_peltier);
    RUN_TEST(test_sim_218_05_unsafe_persistence_locks_peltier);
    RUN_TEST(test_sim_218_06_fault_phase_locks_peltier);
    RUN_TEST(test_sim_218_07_rebegin_tears_down_and_never_reactivates);
    RUN_TEST(test_sim_218_08_final_teardown_ends_open_fan_run_on);
    RUN_TEST(test_sim_218_09_product_default_is_fail_closed);
    RUN_TEST(test_sim_218_10_no_ports_no_chain_and_late_attach_rejected);
    RUN_TEST(test_s6_trip_locks_peltier_is_visible_and_latched);
    RUN_TEST(test_s6_trip_keeps_the_planner_outer_fan_run_on);
    RUN_TEST(test_s6_stale_raw_samples_block_without_latching);
    RUN_TEST(test_s6_unconfigured_thermal_safety_never_releases_the_peltier);
    RUN_TEST(test_s6_configuration_is_refused_during_an_active_run);
    RUN_TEST(
        test_s6_a_refused_configuration_does_not_reset_the_invalid_episode);
    RUN_TEST(test_s6_a_refused_configuration_keeps_the_trend_history);
    RUN_TEST(
        test_s6_clear_during_a_run_blocks_the_peltier_and_cannot_restart_silently);
    RUN_TEST(test_s6_configuration_without_a_run_state_is_refused);
    RUN_TEST(test_s2_start_admission_matrix);
    RUN_TEST(test_s2_state_jump_between_prepare_and_confirm_is_refused);
    RUN_TEST(
        test_s2_program_start_is_refused_before_the_command_identity_is_used);
    return UNITY_END();
}
