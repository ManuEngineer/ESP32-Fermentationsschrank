#include <unity.h>

#include <optional>

#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"
#include "virtual_time_source.hpp"

// R1 trial plan S4: the narrow application writer for the persisted actuator
// planner parameters (owner decision D2). TEST fixtures only.

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    static FermentationApplicationRequestResult prepareStartManualTimedBody(
        FermentationApplication& application,
        const FermentationUiCommandContext& context,
        const ManualTimedRunValues& values) {
        return application.prepareStartManualTimedUnguarded(context, values);
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;

ActuatorPlannerParameters validParameters(std::uint64_t outerPostRun = 3'000U) {
    ActuatorPlannerParameters parameters;
    parameters.switchingWindowMillis = 10'000U;
    parameters.minimumOnMillis = 2'000U;
    parameters.minimumOffMillis = 1'000U;
    parameters.polarityDeadTimeMillis = 3'000U;
    parameters.pulseAccumulatorCapMillis = 10'000U;
    parameters.counterDirectionConfirmationQuoteThreshold = 0.5;
    parameters.counterDirectionConfirmationDurationMillis = 2'000U;
    parameters.requestWatchdogMillis = 60'000U;
    parameters.outerFanPostRunMillis = outerPostRun;
    parameters.innerFanPostRunMillis = 1'000U;
    return parameters;
}

CrossRolePlausibilityContext validEvidence() {
    CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

void assertActivated(const ApplicationConfigurationChangeResult& result) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(result.preview));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(result.commit));
}

struct Fixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    FermentationApplication application;

    Fixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
        application.publishOwningRuntimeEvidence(validEvidence());
    }
    std::optional<ServiceConfigurationRevision> revision() {
        const auto snapshot = application.actuatorPlannerParameters();
        TEST_ASSERT_TRUE(snapshot.has_value());
        return snapshot->revision;
    }
    ApplicationConfigurationChangeResult apply(
        const std::optional<ActuatorPlannerParameters>& value) {
        return application.applyActuatorPlannerParameters(value, revision());
    }
    void startRun() {
        ManualTimedRunValues values;
        values.targetTemperatureCelsius = 30.0;
        values.durationMinutes = 60U;
        values.qualificationBandCelsius = 0.5;
        values.qualificationDurationMinutes = 10U;
        values.maximumTargetReachMinutes = 180U;
        FermentationUiCommandContext context;
        context.expected = application.uiSnapshot().revisions;
        context.monotonicMillis = timeSource.monotonicMillis();
        const auto prepared =
            FermentationApplicationTestAccess::prepareStartManualTimedBody(
                application, context, values);
        TEST_ASSERT_TRUE(prepared.request.has_value());
        const auto confirmed = application.confirmPrepared(prepared);
        TEST_ASSERT_TRUE(confirmed.request.has_value());
        static_cast<void>(
            application.applyConfirmedPrepared(*confirmed.request));
        TEST_ASSERT_TRUE(application.uiSnapshot().home.mode ==
                         FermentationHomeMode::ActiveRun);
    }
};

void test_default_is_absent() {
    Fixture fixture;
    const auto snapshot = fixture.application.actuatorPlannerParameters();
    TEST_ASSERT_TRUE(snapshot.has_value());
    TEST_ASSERT_FALSE(snapshot->parameters.has_value());
}

void test_valid_parameters_are_stored_and_read_back() {
    Fixture fixture;
    assertActivated(fixture.apply(validParameters()));
    const auto snapshot = fixture.application.actuatorPlannerParameters();
    TEST_ASSERT_TRUE(snapshot->parameters.has_value());
    TEST_ASSERT_TRUE(*snapshot->parameters == validParameters());
}

void test_invalid_and_unconfigured_parameters_are_refused_without_a_slot() {
    Fixture fixture;
    const auto before = fixture.revision();
    ActuatorPlannerParameters unconfigured;  // all zero = Unconfigured
    auto zeroPostRun = validParameters();
    zeroPostRun.outerFanPostRunMillis = 0U;  // Invalid (post-run is mandatory)
    auto badThreshold = validParameters();
    badThreshold.counterDirectionConfirmationQuoteThreshold = 1.5;
    for (const auto& invalid : {unconfigured, zeroPostRun, badThreshold}) {
        const auto result = fixture.apply(invalid);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ConfigurationPreviewStatus::InvalidCandidate),
            static_cast<int>(result.preview));
        TEST_ASSERT_FALSE(fixture.application.actuatorPlannerParameters()
                              ->parameters.has_value());
        TEST_ASSERT_TRUE(fixture.revision() == before);
    }
    // No preview slot stayed occupied.
    assertActivated(fixture.apply(validParameters()));
}

void test_missing_and_stale_revisions_are_rejected() {
    Fixture fixture;
    const auto first = fixture.revision();
    const auto missing = fixture.application.applyActuatorPlannerParameters(
        validParameters(), std::nullopt);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::StateChanged),
        static_cast<int>(missing.preview));
    assertActivated(fixture.application.applyActuatorPlannerParameters(
        validParameters(), first));
    const auto stale = fixture.application.applyActuatorPlannerParameters(
        validParameters(4'000U), first);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::StateChanged),
        static_cast<int>(stale.preview));
    TEST_ASSERT_TRUE(
        *fixture.application.actuatorPlannerParameters()->parameters ==
        validParameters());
}

// Semantic cleanup: clear -> nullopt with an advanced revision (no claim about
// identical partition bytes).
void test_clear_returns_to_absent_with_an_advanced_revision() {
    Fixture fixture;
    const auto start = fixture.revision();
    assertActivated(fixture.apply(validParameters()));
    const auto withValues = fixture.revision();
    TEST_ASSERT_TRUE(withValues != start);
    assertActivated(fixture.apply(std::nullopt));
    const auto snapshot = fixture.application.actuatorPlannerParameters();
    TEST_ASSERT_FALSE(snapshot->parameters.has_value());
    TEST_ASSERT_TRUE(snapshot->revision != withValues);
    TEST_ASSERT_TRUE(snapshot->revision != start);
}

void test_changes_are_refused_while_a_run_is_active() {
    Fixture fixture;
    fixture.startRun();
    const auto before = fixture.revision();
    const auto result = fixture.apply(validParameters());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::NotAllowed),
        static_cast<int>(result.preview));
    TEST_ASSERT_FALSE(fixture.application.actuatorPlannerParameters()
                          ->parameters.has_value());
    TEST_ASSERT_TRUE(fixture.revision() == before);
}

void test_not_started_application_refuses_and_has_no_snapshot() {
    FermentationApplication application;
    TEST_ASSERT_FALSE(application.actuatorPlannerParameters().has_value());
    const auto result = application.applyActuatorPlannerParameters(
        validParameters(), std::optional<ServiceConfigurationRevision>{
                               ServiceConfigurationRevision{}});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable),
        static_cast<int>(result.preview));
}

void test_parameters_persist_across_a_restart_and_clear_persists_too() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    {
        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
        assertActivated(application.applyActuatorPlannerParameters(
            validParameters(),
            application.actuatorPlannerParameters()->revision));
    }
    {
        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        FermentationApplication restarted;
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            restarted.begin(platform, store, timeZoneResolver, timeSource));
        const auto snapshot = restarted.actuatorPlannerParameters();
        TEST_ASSERT_TRUE(snapshot->parameters.has_value());
        TEST_ASSERT_TRUE(*snapshot->parameters == validParameters());
        assertActivated(restarted.applyActuatorPlannerParameters(
            std::nullopt, snapshot->revision));
    }
    device_platform::DevicePlatform platform;
    device_platform::VirtualTimeSource timeSource;
    FermentationApplication again;
    TEST_ASSERT_TRUE(platform.begin({true}));
    timeSource.setUnixTimeSeconds(1'700'000'000LL);
    TEST_ASSERT_TRUE(
        again.begin(platform, store, timeZoneResolver, timeSource));
    TEST_ASSERT_FALSE(
        again.actuatorPlannerParameters()->parameters.has_value());
}

// The existing factory reset removes the slot with the rest of the service
// configuration (the trial values never survive a reset).
void test_factory_reset_removes_the_stored_parameters() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    assertActivated(application.applyActuatorPlannerParameters(
        validParameters(), application.actuatorPlannerParameters()->revision));
    TEST_ASSERT_TRUE(
        application.actuatorPlannerParameters()->parameters.has_value());
    const auto reset = application.beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(reset.status));
    const auto snapshot = application.actuatorPlannerParameters();
    TEST_ASSERT_TRUE(snapshot.has_value());
    TEST_ASSERT_FALSE(snapshot->parameters.has_value());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_absent);
    RUN_TEST(test_valid_parameters_are_stored_and_read_back);
    RUN_TEST(
        test_invalid_and_unconfigured_parameters_are_refused_without_a_slot);
    RUN_TEST(test_missing_and_stale_revisions_are_rejected);
    RUN_TEST(test_clear_returns_to_absent_with_an_advanced_revision);
    RUN_TEST(test_changes_are_refused_while_a_run_is_active);
    RUN_TEST(test_not_started_application_refuses_and_has_no_snapshot);
    RUN_TEST(test_parameters_persist_across_a_restart_and_clear_persists_too);
    RUN_TEST(test_factory_reset_removes_the_stored_parameters);
    return UNITY_END();
}
