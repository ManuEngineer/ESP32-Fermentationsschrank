#include <unity.h>

#include <cstring>

#include "app_config.hpp"
#include "device_platform.hpp"
#include "mock_reset_cause_source.hpp"
#include "mock_time_zone_resolver.hpp"
#include "fermentation_application.hpp"
#include "run_commands.hpp"
#include "run_persistence_coordinator.hpp"
#include "standard_program_catalog.hpp"
#include "simulated_persistent_state_store.hpp"

void test_project_metadata() {
    TEST_ASSERT_EQUAL_STRING("ESP32-Fermentationsschrank",
                             app_config::kProjectName);
    TEST_ASSERT_EQUAL_STRING("ManuEngineer", app_config::kProjectOwner);
    TEST_ASSERT_GREATER_THAN(0, std::strlen(app_config::kFirmwareVersion));
}

void test_release_1_resource_invariants() {
    TEST_ASSERT_EQUAL_UINT32(4U, app_config::kTargetFlashMegabytes);
    TEST_ASSERT_EQUAL_UINT32(4U * 1024U * 1024U, app_config::kTargetFlashBytes);
    TEST_ASSERT_FALSE(app_config::kRequiresPsram);
    TEST_ASSERT_FALSE(app_config::kWebOtaEnabled);
    TEST_ASSERT_FALSE(app_config::kRealActuatorsEnabledByDefault);
}

void test_bringup_starts_hardware_unverified_and_locked() {
    const auto& policy = app_config::kEsp32BringupProfilePolicy;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(app_config::BuildProfile::Esp32Bringup),
        static_cast<int>(policy.profile));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(app_config::HardwareState::HardwareUnverified),
        static_cast<int>(policy.startupHardwareState));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(app_config::ActuatorPolicy::LockedForBringup),
        static_cast<int>(policy.actuatorPolicy));
    TEST_ASSERT_TRUE(policy.bringupDiagnostics);
    TEST_ASSERT_FALSE(policy.releasePolicy);
    TEST_ASSERT_FALSE(policy.realActuatorsEnabled);
    TEST_ASSERT_TRUE(app_config::hasSafeDefaults(policy));
}

void test_release_requires_verification_without_enabling_actuators() {
    const auto& policy = app_config::kEsp32ReleaseProfilePolicy;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(app_config::BuildProfile::Esp32Release),
        static_cast<int>(policy.profile));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(app_config::HardwareState::HardwareUnverified),
        static_cast<int>(policy.startupHardwareState));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(app_config::ActuatorPolicy::RequireVerifiedHardware),
        static_cast<int>(policy.actuatorPolicy));
    TEST_ASSERT_FALSE(policy.bringupDiagnostics);
    TEST_ASSERT_TRUE(policy.releasePolicy);
    TEST_ASSERT_FALSE(policy.webOtaEnabled);
    TEST_ASSERT_FALSE(policy.realActuatorsEnabled);
    TEST_ASSERT_TRUE(app_config::hasSafeDefaults(policy));
}

void test_release_policy_with_non_unverified_startup_state_is_unsafe() {
    auto policy = app_config::kEsp32ReleaseProfilePolicy;
    policy.startupHardwareState = app_config::HardwareState::NativeSimulation;

    TEST_ASSERT_FALSE(app_config::hasSafeDefaults(policy));
}

void test_native_profile_has_no_hardware_target() {
    const auto& policy = app_config::kActiveProfilePolicy;

    TEST_ASSERT_EQUAL_INT(static_cast<int>(app_config::BuildProfile::Native),
                          static_cast<int>(policy.profile));
    TEST_ASSERT_FALSE(policy.esp32Target);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(app_config::ActuatorPolicy::SimulationOnly),
        static_cast<int>(policy.actuatorPolicy));
    TEST_ASSERT_TRUE(app_config::hasSafeDefaults(policy));
}

void test_application_rejects_platform_before_startup() {
    device_platform::DevicePlatform platform;
    fermentation::FermentationApplication application;

    TEST_ASSERT_FALSE(platform.ready());
    TEST_ASSERT_FALSE(application.begin(platform));
    TEST_ASSERT_FALSE(application.ready());
}

void test_platform_rejects_unsafe_startup_context() {
    device_platform::DevicePlatform platform;
    const device_platform::PlatformStartupContext startupContext{false};

    TEST_ASSERT_FALSE(platform.begin(startupContext));
    TEST_ASSERT_FALSE(platform.ready());
}

void test_application_starts_through_platform_interface() {
    device_platform::DevicePlatform platform;
    fermentation::FermentationApplication application;
    const device_platform::PlatformStartupContext startupContext{
        app_config::hasSafeDefaults(app_config::kActiveProfilePolicy),
    };

    TEST_ASSERT_TRUE(platform.begin(startupContext));
    TEST_ASSERT_TRUE(application.begin(platform));
    TEST_ASSERT_TRUE(application.ready());
}

void test_application_accepts_const_reset_cause_source() {
    device_platform::DevicePlatform platform;
    fermentation::FermentationApplication application;
    const device_platform::PlatformStartupContext startupContext{
        app_config::hasSafeDefaults(app_config::kActiveProfilePolicy),
    };
    const device_platform_test_support::MockResetCauseSource resetCauseSource(
        device_platform::ResetCause::PowerOn);

    TEST_ASSERT_TRUE(platform.begin(startupContext));
    TEST_ASSERT_TRUE(application.begin(platform, &resetCauseSource));
    TEST_ASSERT_TRUE(application.presentationState().resetCause.has_value());
    TEST_ASSERT_TRUE(*application.presentationState().resetCause ==
                     device_platform::ResetCause::PowerOn);
}

void test_product_application_composes_against_abstract_ports() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    const device_platform::PlatformStartupContext startupContext{true};

    TEST_ASSERT_TRUE(platform.begin(startupContext));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ApplicationLifecycleState::Ready),
        static_cast<int>(application.lifecycleState()));
    const auto published = application.publishedProcessState();
    TEST_ASSERT_TRUE(published.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::ProcessState::Standby),
                          static_cast<int>(published->state));
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(published->transitionSequence));
}

void test_product_application_rejects_unready_platform_before_lifecycle() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_FALSE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ApplicationLifecycleState::Initializing),
        static_cast<int>(application.lifecycleState()));
    TEST_ASSERT_FALSE(application.publishedProcessState().has_value());
}

void test_product_application_requires_service_when_runtime_lease_is_denied() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    timeZoneResolver.setStatus(
        device_platform::TimeZonePrepareStatus::PreparationFailed);
    fermentation::FermentationApplication application;
    const device_platform::PlatformStartupContext startupContext{true};

    TEST_ASSERT_TRUE(platform.begin(startupContext));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_FALSE(application.ready());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ApplicationLifecycleState::ServiceRequired),
        static_cast<int>(application.lifecycleState()));
    TEST_ASSERT_FALSE(application.publishedProcessState().has_value());
    TEST_ASSERT_FALSE(
        application.presentationState().applicationAllocationFailure);
}

void test_product_application_keeps_resume_offer_after_begin_returns() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    fermentation::RunPersistenceCoordinator seed(
        store, device_platform::StorageEpoch(1U),
        fermentation::RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::RunPersistenceLoadStatus::NoPersistedRun),
        static_cast<int>(seed.loadAndInitialize().status));

    auto document = fermentation::FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(document.has_value());
    document->program.productSensorFailure.fallbackDelaySeconds = 30U;
    document->program.fermentationStages.front().targetTemperatureCelsius =
        38.0;
    document->program.fermentationStages.front().durationMinutes = 120U;
    document->program.targetQualification.bandCelsius = 0.5;
    document->program.targetQualification.durationMinutes = 10U;
    document->program.maximumTargetReachMinutes = 180U;
    document->program.preheat = true;
    document->program.maximumProductWaitMinutes = 30U;
    TEST_ASSERT_TRUE(fermentation::validateProgram(*document).valid());
    fermentation::RunCommandState state;
    state.processState.state = fermentation::ProcessState::Standby;
    fermentation::ProgramStartRequest request;
    request.envelope = {42U,
                        fermentation::CommandSource::LocalDisplay,
                        100U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.runId = "resume-offer";
    request.program = *document;
    request.sourceProgramRevision =
        ::fermentation::RunProgramSourceRevision{1U};
    request.sensorMode = fermentation::RunSensorMode::Product;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = true;
    const auto decision = fermentation::decideProgramStart(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunPersistenceResultStatus::Applied),
        static_cast<int>(seed.persistCommand(state, decision,
                                             fermentation::RunCheckpointTime{
                                                 100U, 1'700'000'000LL})
                             .status));
    store.restart();

    device_platform::DevicePlatform platform;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ApplicationLifecycleState::Ready),
        static_cast<int>(application.lifecycleState()));
    TEST_ASSERT_FALSE(application.publishedProcessState().has_value());
    TEST_ASSERT_TRUE(application.presentationState().faultCode ==
                     fermentation::FaultCode::None);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_project_metadata);
    RUN_TEST(test_release_1_resource_invariants);
    RUN_TEST(test_bringup_starts_hardware_unverified_and_locked);
    RUN_TEST(test_release_requires_verification_without_enabling_actuators);
    RUN_TEST(test_release_policy_with_non_unverified_startup_state_is_unsafe);
    RUN_TEST(test_native_profile_has_no_hardware_target);
    RUN_TEST(test_application_rejects_platform_before_startup);
    RUN_TEST(test_platform_rejects_unsafe_startup_context);
    RUN_TEST(test_application_starts_through_platform_interface);
    RUN_TEST(test_application_accepts_const_reset_cause_source);
    RUN_TEST(test_product_application_composes_against_abstract_ports);
    RUN_TEST(
        test_product_application_rejects_unready_platform_before_lifecycle);
    RUN_TEST(
        test_product_application_requires_service_when_runtime_lease_is_denied);
    RUN_TEST(test_product_application_keeps_resume_offer_after_begin_returns);
    return UNITY_END();
}
