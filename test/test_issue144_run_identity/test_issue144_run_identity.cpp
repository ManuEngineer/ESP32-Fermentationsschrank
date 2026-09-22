#include <unity.h>

#include <array>
#include <limits>
#include <mutex>
#include <optional>
#include <utility>
#include <variant>

#include "application_run_identity.hpp"
#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_limits.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "mock_time_zone_resolver.hpp"
#include "run_persistence_coordinator.hpp"
#include "sensor_quality_config.hpp"
#include "sensor_quality_pipeline.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "standard_program_catalog.hpp"
#include "simulated_persistent_state_store.hpp"
#include "temperature_source.hpp"

namespace fermentation {

class ConfigurationServiceTestAccess {
   public:
    static void forceRuntimeFailure(ConfigurationService& service) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        service.enterFailClosedLocked(
            ConfigurationServiceMode::RuntimeFailure,
            ConfigurationRuntimeFailureCause::ServiceStateInvariantViolation);
    }
};

class RunPersistenceCoordinatorTestAccess {
   public:
    static void setState(RunPersistenceCoordinator& coordinator,
                         RunPersistenceCoordinatorState state) {
        coordinator.state_ = state;
    }
};

class FermentationApplicationTestAccess {
   public:
    static bool applicationReadiness(
        const FermentationApplication& application) {
        return application.applicationReadiness();
    }

    static ConfigurationService& configurationService(
        FermentationApplication& application) {
        return *application.configurationService_;
    }

    static RunPersistenceCoordinator& runPersistenceCoordinator(
        FermentationApplication& application) {
        return *application.runPersistenceCoordinator_;
    }

    static void setLifecycleState(FermentationApplication& application,
                                  ApplicationLifecycleState state) {
        application.lifecycleState_ = state;
    }

    static void setStorageEpoch(FermentationApplication& application,
                                device_platform::StorageEpoch epoch) {
        application.storageEpoch_ = epoch;
    }

    static void setLoadStatus(FermentationApplication& application,
                              RunPersistenceLoadStatus status) {
        application.persistenceLoadStatus_ = status;
    }

    static void setLoadDisposition(FermentationApplication& application,
                                   RunLoadDisposition disposition) {
        application.loadDisposition_ = disposition;
    }

    static void setCriticalSafetyEventPending(
        FermentationApplication& application, bool pending) {
        application.runtimeRunState_->criticalSafetyEventPending = pending;
    }
};

class ApplicationRunIdentityTestAccess {
   public:
    static ApplicationCommandIdentity allocate(ApplicationRunIdentity& value) {
        return *value.allocateForApplication().identity;
    }
    static ApplicationCommandIdentityResult allocateResult(
        ApplicationRunIdentity& value) {
        return value.allocateForApplication();
    }
    static std::optional<std::string> makeRunId(
        const ApplicationRunIdentity& value, CommandId commandId) {
        return value.makeRunId(commandId);
    }
};

}  // namespace fermentation

namespace {

using device_platform::StorageEpoch;
using fermentation::ApplicationRunIdentity;
using fermentation::ApplicationRunIdentityTestAccess;
using fermentation::CommandId;
using fermentation::ProgramCatalogRevision;

fermentation::CrossRolePlausibilityContext owningEvidence();

class FailNextRunSlotStore final : public device_platform::IStateStore {
   public:
    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override {
        if (!failNextRunSlotKey_.empty() &&
            key.bytes() == failNextRunSlotKey_) {
            failNextRunSlotKey_.clear();
            return device_platform::StateStoreWriteStatus::WriteError;
        }
        return backing_.write(key, value);
    }

    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override {
        return backing_.read(key, maxBytes);
    }

    void failNextRunSlot(const char* key = "rc0") { failNextRunSlotKey_ = key; }
    void restart() { backing_.restart(); }

   private:
    device_platform_test_support::SimulatedPersistentStateStore backing_;
    std::string failNextRunSlotKey_;
};

void test_empty_identity_space_allocates_from_one() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{7U}, std::optional<CommandId>{CommandId{0U}});
    TEST_ASSERT_TRUE(identity.has_value());

    auto allocator = std::move(*identity);
    const auto first = ApplicationRunIdentityTestAccess::allocate(allocator);
    TEST_ASSERT_EQUAL_UINT64(1U, first.commandId());
    TEST_ASSERT_EQUAL_UINT64(1U, first.uiRequestId().value);
    const auto runId = ApplicationRunIdentityTestAccess::makeRunId(
        allocator, first.commandId());
    TEST_ASSERT_TRUE(runId.has_value());
    TEST_ASSERT_EQUAL_STRING("e7-c1", runId->c_str());
}

void test_identity_uses_committed_high_water_and_rejects_invalid_bases() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{3U}, std::optional<CommandId>{CommandId{41U}});
    TEST_ASSERT_TRUE(identity.has_value());
    auto allocator = std::move(*identity);
    TEST_ASSERT_EQUAL_UINT64(
        42U, ApplicationRunIdentityTestAccess::allocate(allocator).commandId());

    TEST_ASSERT_FALSE(ApplicationRunIdentity::create(
                          StorageEpoch{}, std::optional<CommandId>{0U})
                          .has_value());
    TEST_ASSERT_FALSE(
        ApplicationRunIdentity::create(StorageEpoch{3U}, std::nullopt)
            .has_value());
}

void test_identity_overflow_does_not_wrap_or_issue_zero() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{1U},
        std::optional<CommandId>{std::numeric_limits<CommandId>::max()});
    TEST_ASSERT_TRUE(identity.has_value());
    auto allocator = std::move(*identity);
    const auto allocation =
        ApplicationRunIdentityTestAccess::allocateResult(allocator);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ApplicationRunIdentityStatus::Overflow),
        static_cast<int>(allocation.status));
    TEST_ASSERT_FALSE(allocation.identity.has_value());
}

void test_catalog_revision_maps_to_neutral_run_provenance_without_truncation() {
    const auto mapped = fermentation::makeRunProgramSourceRevision(
        ProgramCatalogRevision{0x1'0000'0000ULL + 9U});
    TEST_ASSERT_TRUE(mapped.has_value());
    TEST_ASSERT_EQUAL_UINT64(0x1'0000'0000ULL + 9U, mapped->value());
    TEST_ASSERT_FALSE(
        fermentation::makeRunProgramSourceRevision(ProgramCatalogRevision{})
            .has_value());
}

void test_ui_id_is_application_bound_to_existing_command_envelope() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    application.publishOwningRuntimeEvidence(owningEvidence());

    fermentation::FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::WebInterface;
    context.monotonicMillis = 100U;
    fermentation::FermentationUiStartManualHoldingIntent manual;
    manual.plan.targetTemperatureCelsius = 30.0;
    const auto prepared =
        application.prepareStartManualHolding(context, manual);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_TRUE(prepared.uiRequestId.has_value());
    TEST_ASSERT_EQUAL_UINT64(prepared.uiRequestId->value,
                             prepared.request->commandEnvelope().id);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::CommandSource::WebInterface),
        static_cast<int>(prepared.request->commandEnvelope().source));
}

void test_application_prepares_manual_timed_with_shared_identity() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    application.publishOwningRuntimeEvidence(owningEvidence());

    fermentation::FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::LocalDisplay;
    context.monotonicMillis = 100U;
    fermentation::ManualTimedRunValues values;
    values.targetTemperatureCelsius = 30.0;
    values.durationMinutes = 60U;
    values.sensorMode = fermentation::RunSensorMode::Product;
    values.preheatEnabled = true;
    values.maximumProductWaitMinutes = 30U;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;

    const auto prepared = application.prepareStartManualTimed(context, values);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(prepared.status));
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_TRUE(prepared.request->runId().has_value());
    TEST_ASSERT_EQUAL_STRING("e1-c1", prepared.request->runId()->c_str());

    const auto confirmed = application.confirmPrepared(prepared);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(confirmed.status));
    TEST_ASSERT_EQUAL_UINT64(prepared.request->commandId(),
                             confirmed.request->commandId());
    TEST_ASSERT_EQUAL_STRING(prepared.request->runId()->c_str(),
                             confirmed.request->runId()->c_str());
    TEST_ASSERT_FALSE(prepared.request->commandEnvelope().confirmed);
    TEST_ASSERT_TRUE(confirmed.request->commandEnvelope().confirmed);

    auto staleEvidence = owningEvidence();
    staleEvidence.product.quality = device_platform::SensorQuality::Stale;
    application.publishOwningRuntimeEvidence(staleEvidence);
    const auto rejected = application.confirmPrepared(prepared);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Unavailable),
        static_cast<int>(rejected.status));
    TEST_ASSERT_FALSE(rejected.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e1-c1", prepared.request->runId()->c_str());
    TEST_ASSERT_FALSE(prepared.request->commandEnvelope().confirmed);

    application.publishOwningRuntimeEvidence(owningEvidence());
    const auto preparedAgain =
        application.prepareStartManualTimed(context, values);
    TEST_ASSERT_TRUE(preparedAgain.request.has_value());
    auto failedEvidence = owningEvidence();
    failedEvidence.product.quality = device_platform::SensorQuality::Failed;
    application.publishOwningRuntimeEvidence(failedEvidence);
    const auto failed = application.confirmPrepared(preparedAgain);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Unavailable),
        static_cast<int>(failed.status));
    TEST_ASSERT_FALSE(failed.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(2U, preparedAgain.request->commandId());
}

void test_application_projects_default_runtime_evidence_fail_closed() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));

    const auto snapshot = application.uiSnapshot();
    TEST_ASSERT_TRUE(snapshot.status.ready);
    TEST_ASSERT_FALSE(snapshot.service.available);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkMode::UNSELECTED),
        static_cast<int>(snapshot.network.currentMode));
    TEST_ASSERT_EQUAL_UINT32(3U, snapshot.temperatures.size());
    for (const auto& temperature : snapshot.temperatures) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(device_platform::SensorQuality::Stale),
            static_cast<int>(temperature.quality.quality));
        TEST_ASSERT_FALSE(temperature.valueCelsius.has_value());
    }
    TEST_ASSERT_TRUE(snapshot.refreshRevision.has_value());
}

void test_application_readiness_rejects_each_plan_condition() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(
        fermentation::FermentationApplicationTestAccess::applicationReadiness(
            application));

    fermentation::FermentationApplicationTestAccess::setLifecycleState(
        application, fermentation::ApplicationLifecycleState::Initializing);
    TEST_ASSERT_FALSE(
        fermentation::FermentationApplicationTestAccess::applicationReadiness(
            application));
    fermentation::FermentationApplicationTestAccess::setLifecycleState(
        application, fermentation::ApplicationLifecycleState::Ready);

    auto& configuration =
        fermentation::FermentationApplicationTestAccess::configurationService(
            application);
    std::array<
        fermentation::RuntimeConfigurationReadResult,
        fermentation::configuration_limits::kMaxRuntimeConfigurationReadLeases>
        leases{};
    bool allLeasesGranted = true;
    for (auto& result : leases) {
        result = configuration.acquireRuntime();
        allLeasesGranted = allLeasesGranted &&
                           result.status ==
                               fermentation::RuntimeConfigurationReadStatus::
                                   RuntimeLeaseGranted;
    }
    TEST_ASSERT_TRUE(allLeasesGranted);
    TEST_ASSERT_FALSE(
        fermentation::FermentationApplicationTestAccess::applicationReadiness(
            application));
    leases = {};

    device_platform::DevicePlatform unavailablePlatform;
    device_platform_test_support::SimulatedPersistentStateStore
        unavailableStore;
    fermentation::FermentationApplication unavailableApplication;
    TEST_ASSERT_TRUE(unavailablePlatform.begin({true}));
    TEST_ASSERT_TRUE(unavailableApplication.begin(
        unavailablePlatform, unavailableStore, timeZoneResolver));
    auto& unavailableConfiguration =
        fermentation::FermentationApplicationTestAccess::configurationService(
            unavailableApplication);
    fermentation::ConfigurationServiceTestAccess::forceRuntimeFailure(
        unavailableConfiguration);
    TEST_ASSERT_FALSE(
        fermentation::FermentationApplicationTestAccess::applicationReadiness(
            unavailableApplication));

    device_platform::DevicePlatform epochPlatform;
    device_platform_test_support::SimulatedPersistentStateStore epochStore;
    fermentation::FermentationApplication epochApplication;
    TEST_ASSERT_TRUE(epochPlatform.begin({true}));
    TEST_ASSERT_TRUE(
        epochApplication.begin(epochPlatform, epochStore, timeZoneResolver));
    fermentation::FermentationApplicationTestAccess::setStorageEpoch(
        epochApplication, device_platform::StorageEpoch{2U});
    TEST_ASSERT_FALSE(
        fermentation::FermentationApplicationTestAccess::applicationReadiness(
            epochApplication));

    const std::array rejectedLoadStatuses = {
        fermentation::RunPersistenceLoadStatus::PreparedInterrupted,
        fermentation::RunPersistenceLoadStatus::NotReconstructible,
        fermentation::RunPersistenceLoadStatus::NotReconstructibleOrphanedState,
        fermentation::RunPersistenceLoadStatus::ReadFailed,
        fermentation::RunPersistenceLoadStatus::CapacityExceeded,
        fermentation::RunPersistenceLoadStatus::UnsupportedSchema,
        fermentation::RunPersistenceLoadStatus::ForeignEpoch,
        fermentation::RunPersistenceLoadStatus::AlreadyInitialized};
    for (const auto status : rejectedLoadStatuses) {
        fermentation::FermentationApplicationTestAccess::setLoadStatus(
            application, status);
        TEST_ASSERT_FALSE(fermentation::FermentationApplicationTestAccess::
                              applicationReadiness(application));
    }
    fermentation::FermentationApplicationTestAccess::setLoadStatus(
        application, fermentation::RunPersistenceLoadStatus::NoPersistedRun);

    fermentation::FermentationApplicationTestAccess::setLoadDisposition(
        application, fermentation::RunLoadDisposition::SafeBoot);
    TEST_ASSERT_FALSE(
        fermentation::FermentationApplicationTestAccess::applicationReadiness(
            application));
    fermentation::FermentationApplicationTestAccess::setLoadDisposition(
        application, fermentation::RunLoadDisposition::Standby);

    const std::array rejectedCoordinatorStates = {
        fermentation::RunPersistenceCoordinatorState::Uninitialized,
        fermentation::RunPersistenceCoordinatorState::Busy,
        fermentation::RunPersistenceCoordinatorState::BlockedIndeterminate,
        fermentation::RunPersistenceCoordinatorState::FallbackRecoveryPending,
        fermentation::RunPersistenceCoordinatorState::
            PersistenceCommittedApplyFailed};
    for (const auto state : rejectedCoordinatorStates) {
        fermentation::RunPersistenceCoordinatorTestAccess::setState(
            fermentation::FermentationApplicationTestAccess::
                runPersistenceCoordinator(application),
            state);
        TEST_ASSERT_FALSE(fermentation::FermentationApplicationTestAccess::
                              applicationReadiness(application));
    }
    fermentation::RunPersistenceCoordinatorTestAccess::setState(
        fermentation::FermentationApplicationTestAccess::
            runPersistenceCoordinator(application),
        fermentation::RunPersistenceCoordinatorState::ReadyEmpty);

    fermentation::FermentationApplicationTestAccess::
        setCriticalSafetyEventPending(application, true);
    TEST_ASSERT_FALSE(
        fermentation::FermentationApplicationTestAccess::applicationReadiness(
            application));
}

void test_confirmation_preserves_canonical_product_selection_paths() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));

    auto productUnavailable = owningEvidence();
    productUnavailable.product.quality = device_platform::SensorQuality::Stale;
    application.publishOwningRuntimeEvidence(productUnavailable);

    fermentation::FermentationUiCommandContext context;
    context.monotonicMillis = 100U;
    fermentation::ManualTimedRunValues values;
    values.targetTemperatureCelsius = 30.0;
    values.durationMinutes = 60U;
    values.sensorMode = fermentation::RunSensorMode::Product;
    values.preheatEnabled = true;
    values.maximumProductWaitMinutes = 30U;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;

    const auto prepared = application.prepareStartManualTimed(context, values);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    const auto confirmed = application.confirmPrepared(prepared);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(confirmed.status));
    TEST_ASSERT_TRUE(confirmed.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(prepared.request->commandId(),
                             confirmed.request->commandId());
    TEST_ASSERT_EQUAL_STRING(prepared.request->runId()->c_str(),
                             confirmed.request->runId()->c_str());
}

void test_pipeline_handoff_drives_application_snapshot_and_confirmation() {
    const auto config = device_platform::SensorQualityConfig::create(
        3U, 5.0, -20.0, 80.0, 5.0, 10'000U, 3U, 2U, 2'000U);
    TEST_ASSERT_TRUE(config.config.has_value());
    device_platform::SensorQualityPipeline air(*config.config);
    device_platform::SensorQualityPipeline product(*config.config);
    device_platform::SensorQualityPipeline cooling(*config.config);
    const auto validReading = [](std::uint64_t timestamp, double celsius) {
        return device_platform::TemperatureReading::create(
                   std::nullopt, timestamp,
                   device_platform::TemperatureSampleStatus::Ok, celsius)
            .reading.value();
    };
    for (auto* pipeline : {&air, &product, &cooling}) {
        (void)pipeline->ingest(validReading(0U, 20.0), 0U);
        (void)pipeline->ingest(validReading(2'000U, 21.0), 2'000U);
    }

    fermentation::CrossRolePlausibilityContext valid;
    valid.air = air.snapshot(2'000U);
    valid.product = product.snapshot(2'000U);
    valid.cooling = cooling.snapshot(2'000U);
    TEST_ASSERT_TRUE(valid.air.quality ==
                     device_platform::SensorQuality::Valid);
    TEST_ASSERT_TRUE(valid.product.quality ==
                     device_platform::SensorQuality::Valid);
    TEST_ASSERT_TRUE(valid.cooling.quality ==
                     device_platform::SensorQuality::Valid);

    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    application.publishOwningRuntimeEvidence(valid);
    const auto validSnapshot = application.uiSnapshot();
    for (const auto& temperature : validSnapshot.temperatures) {
        TEST_ASSERT_TRUE(temperature.quality.quality ==
                         device_platform::SensorQuality::Valid);
    }

    fermentation::ManualTimedRunValues values;
    values.targetTemperatureCelsius = 30.0;
    values.durationMinutes = 60U;
    values.sensorMode = fermentation::RunSensorMode::Air;
    values.preheatEnabled = true;
    values.maximumProductWaitMinutes = 30U;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;
    const auto prepared = application.prepareStartManualTimed(
        fermentation::FermentationUiCommandContext{}, values);
    TEST_ASSERT_TRUE(
        fermentation::FermentationApplicationTestAccess::applicationReadiness(
            application));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(prepared.status));
    TEST_ASSERT_TRUE(application.confirmPrepared(prepared).request.has_value());

    const auto failedReading = [](std::uint64_t timestamp) {
        return device_platform::TemperatureReading::create(
                   std::nullopt, timestamp,
                   device_platform::TemperatureSampleStatus::CrcFault,
                   std::nullopt)
            .reading.value();
    };
    for (auto* pipeline : {&air, &product, &cooling}) {
        (void)pipeline->ingest(failedReading(3'000U), 3'000U);
    }
    auto stale = valid;
    stale.air = air.snapshot(3'000U);
    stale.product = product.snapshot(3'000U);
    stale.cooling = cooling.snapshot(3'000U);
    application.publishOwningRuntimeEvidence(stale);
    const auto staleSnapshot = application.uiSnapshot();
    for (const auto& temperature : staleSnapshot.temperatures) {
        TEST_ASSERT_TRUE(temperature.quality.quality ==
                         device_platform::SensorQuality::Stale);
    }
    TEST_ASSERT_FALSE(
        application.confirmPrepared(prepared).request.has_value());

    for (auto* pipeline : {&air, &product, &cooling}) {
        for (std::uint64_t timestamp = 4'000U; timestamp <= 6'000U;
             timestamp += 1'000U) {
            (void)pipeline->ingest(failedReading(timestamp), timestamp);
        }
    }
    auto failed = valid;
    failed.air = air.snapshot(6'000U);
    failed.product = product.snapshot(6'000U);
    failed.cooling = cooling.snapshot(6'000U);
    application.publishOwningRuntimeEvidence(failed);
    const auto failedSnapshot = application.uiSnapshot();
    for (const auto& temperature : failedSnapshot.temperatures) {
        TEST_ASSERT_TRUE(temperature.quality.quality ==
                         device_platform::SensorQuality::Failed);
    }
}

fermentation::CrossRolePlausibilityContext owningEvidence() {
    fermentation::CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

fermentation::FermentationUiManualRunPlanValues coolingValues() {
    fermentation::FermentationUiManualRunPlanValues values;
    values.targetTemperatureCelsius = 8.0;
    values.sensorMode = fermentation::RunSensorMode::Air;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 60U;
    return values;
}

void seedActiveRunForApplication(device_platform::IStateStore& store) {
    fermentation::RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch{1U},
        fermentation::RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::RunPersistenceLoadStatus::NoPersistedRun),
        static_cast<int>(coordinator.loadAndInitialize().status));
    auto program = fermentation::FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(program.has_value());
    program->program.productSensorFailure.fallbackDelaySeconds = 30U;
    program->program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program->program.fermentationStages.front().durationMinutes = 120U;
    program->program.targetQualification.bandCelsius = 0.5;
    program->program.targetQualification.durationMinutes = 10U;
    program->program.maximumTargetReachMinutes = 180U;
    TEST_ASSERT_TRUE(fermentation::validateProgram(*program).valid());

    fermentation::RunCommandState state;
    state.processState.state = fermentation::ProcessState::Standby;
    fermentation::ProgramStartRequest request;
    request.envelope = {11U,
                        fermentation::CommandSource::LocalDisplay,
                        100U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.runId = "e1-c11";
    request.program = *program;
    request.sourceProgramRevision = fermentation::RunProgramSourceRevision{1U};
    request.sensorMode = fermentation::RunSensorMode::Product;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = true;
    const auto decision = fermentation::decideProgramStart(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunPersistenceResultStatus::Applied),
        static_cast<int>(coordinator
                             .persistCommand(state, decision,
                                             fermentation::RunCheckpointTime{
                                                 100U, 1'700'000'000LL})
                             .status));
}

void test_application_composes_all_run_identities_at_one_boundary() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    fermentation::FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::LocalDisplay;
    context.monotonicMillis = 100U;
    context.expected.expectedStateSequence = 0U;
    context.expected.expectedProgramCatalogRevision =
        fermentation::ProgramCatalogRevision{1U};
    const auto evidence = owningEvidence();
    application.publishOwningRuntimeEvidence(evidence);

    fermentation::FermentationUiStartProgramIntent startProgram;
    startProgram.candidate.programId = "water-kefir";
    startProgram.candidate.sensorMode = fermentation::RunSensorMode::Product;
    const auto preparedProgram =
        application.prepareStartProgram(context, startProgram);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::FermentationApplicationRequestStatus::
                             ProgramUnavailable),
        static_cast<int>(preparedProgram.status));
    TEST_ASSERT_FALSE(preparedProgram.request.has_value());
    TEST_ASSERT_FALSE(preparedProgram.uiRequestId.has_value());

    auto staleContext = context;
    staleContext.expected.expectedProgramCatalogRevision =
        fermentation::ProgramCatalogRevision{2U};
    const auto stale =
        application.prepareStartProgram(staleContext, startProgram);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::FermentationApplicationRequestStatus::
                             StaleProgramCatalog),
        static_cast<int>(stale.status));
    TEST_ASSERT_FALSE(stale.request.has_value());

    fermentation::FermentationUiStartManualHoldingIntent manualStart;
    manualStart.plan.targetTemperatureCelsius = 30.0;
    manualStart.plan.sensorMode = fermentation::RunSensorMode::Air;
    const auto preparedManual =
        application.prepareStartManualHolding(context, manualStart);
    TEST_ASSERT_TRUE(preparedManual.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, preparedManual.request->commandId());
    TEST_ASSERT_TRUE(preparedManual.request->runId().has_value());
    TEST_ASSERT_EQUAL_STRING("e1-c1", preparedManual.request->runId()->c_str());

    fermentation::FermentationUiStopRunIntent stop;
    stop.option = fermentation::StopOption::AbortAndTurnOff;
    const auto preparedStop = application.prepareStop(context, stop);
    TEST_ASSERT_TRUE(preparedStop.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(2U, preparedStop.request->commandId());
    TEST_ASSERT_FALSE(preparedStop.request->runId().has_value());

    fermentation::FermentationUiCompleteRunIntent complete;
    const auto preparedComplete =
        application.prepareCompletion(context, complete);
    TEST_ASSERT_TRUE(preparedComplete.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(3U, preparedComplete.request->commandId());
    TEST_ASSERT_FALSE(preparedComplete.request->runId().has_value());

    stop.option = fermentation::StopOption::AbortAndCool;
    stop.coolingPlan = coolingValues();
    const auto preparedCoolingStop = application.prepareStop(context, stop);
    TEST_ASSERT_TRUE(preparedCoolingStop.request.has_value());
    TEST_ASSERT_TRUE(preparedCoolingStop.uiRequestId.has_value());
    TEST_ASSERT_EQUAL_UINT64(4U, preparedCoolingStop.request->commandId());
    TEST_ASSERT_TRUE(preparedCoolingStop.request->runId().has_value());
    TEST_ASSERT_EQUAL_STRING("e1-c4",
                             preparedCoolingStop.request->runId()->c_str());

    complete.startCooling = true;
    complete.coolingPlan = coolingValues();
    const auto preparedCoolingCompletion =
        application.prepareCompletion(context, complete);
    TEST_ASSERT_TRUE(preparedCoolingCompletion.request.has_value());
    TEST_ASSERT_TRUE(preparedCoolingCompletion.uiRequestId.has_value());
    TEST_ASSERT_EQUAL_UINT64(5U,
                             preparedCoolingCompletion.request->commandId());
    TEST_ASSERT_TRUE(preparedCoolingCompletion.request->runId().has_value());
    TEST_ASSERT_EQUAL_STRING(
        "e1-c5", preparedCoolingCompletion.request->runId()->c_str());
}

void test_application_reset_hands_off_existing_run_store_to_new_epoch() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    seedActiveRunForApplication(store);
    store.restart();
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    const auto reset = application.beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(reset.status));
    TEST_ASSERT_TRUE(application.ready());

    fermentation::FermentationUiCommandContext context;
    context.monotonicMillis = 200U;
    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared =
        application.prepareStartManualHolding(context, manual);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e2-c1", prepared.request->runId()->c_str());
}

void test_application_prepares_every_envelope_action_with_one_identity() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    fermentation::FermentationUiCommandContext context;
    context.monotonicMillis = 100U;
    const auto prepare = [&application, &context](const auto& intent) {
        return application.prepareEnvelope(
            context, fermentation::FermentationUiEnvelopePayload{intent});
    };
    const auto adjustment =
        prepare(fermentation::FermentationUiAdjustRunIntent{});
    const auto correction =
        prepare(fermentation::FermentationUiRecoveryTimeCorrectionIntent{12U});
    const auto acknowledgement =
        prepare(fermentation::FermentationUiAcknowledgeMessageIntent{7U});
    const auto mute =
        prepare(fermentation::FermentationUiMuteMessageIntent{7U});
    const auto reset = prepare(fermentation::FermentationUiResetFaultIntent{});
    const auto sensor =
        prepare(fermentation::FermentationUiSensorSelectionIntent{});

    TEST_ASSERT_EQUAL_UINT64(1U, adjustment.request->commandId());
    TEST_ASSERT_EQUAL_UINT64(2U, correction.request->commandId());
    TEST_ASSERT_EQUAL_UINT64(3U, acknowledgement.request->commandId());
    TEST_ASSERT_EQUAL_UINT64(4U, mute.request->commandId());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Unavailable),
        static_cast<int>(reset.status));
    TEST_ASSERT_FALSE(reset.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(5U, sensor.request->commandId());
}

void test_confirmation_reuses_prepared_request_without_reallocation() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;

    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));

    fermentation::FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::WebInterface;
    context.monotonicMillis = 314U;
    context.expected.expectedStateSequence = 2U;
    context.expected.expectedRunRevision = 3U;
    context.expected.expectedMessageRevision = 4U;
    context.expected.expectedFaultRevision = 5U;
    context.expected.expectedRecoveryEpisodeRevision = 6U;

    fermentation::FermentationUiStartManualHoldingIntent manual;
    application.publishOwningRuntimeEvidence(owningEvidence());
    const auto prepared =
        application.prepareStartManualHolding(context, manual);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(prepared.status));
    const auto confirmed = application.confirmPrepared(prepared);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(confirmed.status));
    TEST_ASSERT_EQUAL_UINT64(prepared.request->commandId(),
                             confirmed.request->commandId());
    TEST_ASSERT_EQUAL_STRING(prepared.request->runId()->c_str(),
                             confirmed.request->runId()->c_str());
    const auto& before = prepared.request->commandEnvelope();
    const auto& after = confirmed.request->commandEnvelope();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(before.source),
                          static_cast<int>(after.source));
    TEST_ASSERT_EQUAL_UINT64(before.monotonicMillis, after.monotonicMillis);
    TEST_ASSERT_EQUAL_UINT32(before.expectedStateSequence,
                             after.expectedStateSequence);
    TEST_ASSERT_EQUAL_UINT32(before.expectedRunRevision.value(),
                             after.expectedRunRevision.value());
    TEST_ASSERT_EQUAL_UINT32(before.expectedMessageRevision.value(),
                             after.expectedMessageRevision.value());
    TEST_ASSERT_EQUAL_UINT32(before.expectedFaultRevision.value(),
                             after.expectedFaultRevision.value());
    TEST_ASSERT_EQUAL_UINT32(before.expectedRecoveryEpisodeRevision.value(),
                             after.expectedRecoveryEpisodeRevision.value());
    TEST_ASSERT_FALSE(prepared.request->commandEnvelope().confirmed);
    TEST_ASSERT_TRUE(confirmed.request->commandEnvelope().confirmed);

    const auto next = application.prepareStartManualHolding(
        fermentation::FermentationUiCommandContext{}, manual);
    TEST_ASSERT_EQUAL_UINT64(prepared.request->commandId() + 1U,
                             next.request->commandId());
}

void test_application_reconstructs_reset_handoff_after_run_write_cut() {
    FailNextRunSlotStore store;
    seedActiveRunForApplication(store);
    store.restart();
    store.failNextRunSlot();

    device_platform::DevicePlatform platform;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    const auto interrupted = application.beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             RunPersistenceHandoffUnavailable),
        static_cast<int>(interrupted.status));
    TEST_ASSERT_FALSE(application.ready());

    store.restart();
    device_platform::DevicePlatform rebootedPlatform;
    fermentation::FermentationApplication rebooted;
    TEST_ASSERT_TRUE(rebootedPlatform.begin({true}));
    TEST_ASSERT_TRUE(rebooted.begin(rebootedPlatform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(rebooted.ready());

    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared = rebooted.prepareStartManualHolding(
        fermentation::FermentationUiCommandContext{}, manual);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e2-c1", prepared.request->runId()->c_str());
}

void seedCommittedHandoffWithFinalizedHead(
    device_platform_test_support::SimulatedPersistentStateStore& store,
    device_platform_test_support::MockTimeZoneResolver& timeZoneResolver) {
    fermentation::ConfigurationMutationCoordinator mutationCoordinator;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    fermentation::ConfigurationGraphStore graph(store, timeZoneResolver);
    fermentation::ConfigurationService configuration(mutationCoordinator, graph,
                                                     timeZoneResolver);
    auto recovery = fermentation::ConfigurationRecoveryService::create(
        store, bootstrap, graph, configuration, mutationCoordinator);
    TEST_ASSERT_TRUE(recovery != nullptr);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(recovery->boot().status));
    const auto reset = recovery->beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(reset.status));
    auto proof = recovery->takeAuthorizedRunEpochHandoffProof();
    TEST_ASSERT_TRUE(proof.has_value());

    fermentation::RunPersistenceCoordinator runPersistence(
        store, device_platform::StorageEpoch{2U},
        fermentation::RunCheckpointSchedule{});
    const auto prepared = runPersistence.prepareAuthorizedEpochHandoff(*proof);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunPersistenceResultStatus::Applied),
        static_cast<int>(prepared.persistenceResult.status));
    TEST_ASSERT_TRUE(prepared.evidence.has_value());
    const auto committed =
        recovery->commitAuthorizedRunEpochHandoff(*proof, *prepared.evidence);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::RuntimeReady),
        static_cast<int>(committed.status));
    const auto finalized =
        runPersistence.finalizeAuthorizedEpochHandoff(*proof);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunPersistenceResultStatus::Applied),
        static_cast<int>(finalized.persistenceResult.status));
    TEST_ASSERT_TRUE(finalized.evidence.has_value());
    // Deliberately leave the persistent bootstrap record Committed.  The
    // application must verify the exact target head and consume this phase
    // before publishing Ready or initializing the command allocator.
}

void test_application_finishes_committed_handoff_before_ready() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    seedCommittedHandoffWithFinalizedHead(store, timeZoneResolver);
    store.restart();

    device_platform::DevicePlatform platform;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    fermentation::ConfigurationBootstrapStore bootstrap(store);
    const auto scan = bootstrap.scan();
    TEST_ASSERT_TRUE(scan.loaded.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RunEpochHandoffState::Consumed),
        static_cast<int>(scan.loaded->record.handoff));

    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared = application.prepareStartManualHolding(
        fermentation::FermentationUiCommandContext{}, manual);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e2-c1", prepared.request->runId()->c_str());
}

void test_application_resumes_empty_partial_handoff_before_allocator() {
    FailNextRunSlotStore store;
    device_platform::DevicePlatform platform;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(application.ready());

    // Empty run store: slot 0 is durably prepared, slot 1 fails.  The head
    // must still be absent because Pending never writes it.
    store.failNextRunSlot("rc1");
    const auto interrupted = application.beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             RunPersistenceHandoffUnavailable),
        static_cast<int>(interrupted.status));
    const auto head =
        store.read(*device_platform::StateStoreKey::create("rh0").key, 256U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::NotFound),
        static_cast<int>(head.status));
    const auto firstSlot =
        store.read(*device_platform::StateStoreKey::create("rc0").key, 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(firstSlot.status));

    store.restart();
    device_platform::DevicePlatform rebootedPlatform;
    fermentation::FermentationApplication rebooted;
    TEST_ASSERT_TRUE(rebootedPlatform.begin({true}));
    TEST_ASSERT_TRUE(rebooted.begin(rebootedPlatform, store, timeZoneResolver));
    TEST_ASSERT_TRUE(rebooted.ready());
    fermentation::FermentationUiStartManualHoldingIntent manual;
    const auto prepared = rebooted.prepareStartManualHolding(
        fermentation::FermentationUiCommandContext{}, manual);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_EQUAL_STRING("e2-c1", prepared.request->runId()->c_str());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_identity_space_allocates_from_one);
    RUN_TEST(test_identity_uses_committed_high_water_and_rejects_invalid_bases);
    RUN_TEST(test_identity_overflow_does_not_wrap_or_issue_zero);
    RUN_TEST(
        test_catalog_revision_maps_to_neutral_run_provenance_without_truncation);
    RUN_TEST(test_ui_id_is_application_bound_to_existing_command_envelope);
    RUN_TEST(test_application_prepares_manual_timed_with_shared_identity);
    RUN_TEST(test_application_projects_default_runtime_evidence_fail_closed);
    RUN_TEST(test_application_readiness_rejects_each_plan_condition);
    RUN_TEST(test_confirmation_preserves_canonical_product_selection_paths);
    RUN_TEST(
        test_pipeline_handoff_drives_application_snapshot_and_confirmation);
    RUN_TEST(test_application_composes_all_run_identities_at_one_boundary);
    RUN_TEST(test_application_reset_hands_off_existing_run_store_to_new_epoch);
    RUN_TEST(test_application_prepares_every_envelope_action_with_one_identity);
    RUN_TEST(test_confirmation_reuses_prepared_request_without_reallocation);
    RUN_TEST(test_application_reconstructs_reset_handoff_after_run_write_cut);
    RUN_TEST(test_application_finishes_committed_handoff_before_ready);
    RUN_TEST(test_application_resumes_empty_partial_handoff_before_allocator);
    return UNITY_END();
}
