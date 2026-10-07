#include <unity.h>

#include <type_traits>
#include <utility>
#include <variant>

#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "device_platform.hpp"
#include "mock_time_zone_resolver.hpp"
#include "mock_network_lifecycle.hpp"
#include "mock_secure_random_source.hpp"
#include "simulated_persistent_state_store.hpp"
#include "standard_program_catalog.hpp"
#include "virtual_time_source.hpp"

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    // Manual-run requests are refused on every surface while no owner of the
    // technical run limits exists (O5, #172 S9); these reach the private
    // bodies behind that guard so the downstream decision paths stay covered.
    static FermentationApplicationRequestResult prepareStartManualHoldingBody(
        FermentationApplication& application,
        const FermentationUiCommandContext& context,
        const FermentationUiStartManualHoldingIntent& intent) {
        return application.prepareStartManualHoldingUnguarded(context, intent);
    }
    static FermentationApplicationRequestResult prepareStartManualTimedBody(
        FermentationApplication& application,
        const FermentationUiCommandContext& context,
        const ManualTimedRunValues& values) {
        return application.prepareStartManualTimedUnguarded(context, values);
    }
    static bool rename(FermentationApplication& application,
                       const char* deviceName) {
        auto build = application.configurationService_->beginPreview();
        if (build.status != ConfigurationPreviewStatus::Success ||
            !build.lease.valid()) {
            return false;
        }
        build.lease.userConfiguration().deviceName = deviceName;
        const auto installed =
            application.configurationService_->installPreview(
                std::move(build.lease), {ChangeOriginKind::LocalDisplay, 2U},
                {ChangeOperationKind::NormalEdit, 1U});
        if (installed.status != ConfigurationPreviewStatus::Success ||
            !installed.preview.has_value()) {
            return false;
        }
        const auto committed =
            application.configurationService_->confirmPreview(
                installed.preview->handle);
        return committed.status == ConfigurationCommitStatus::Activated ||
               committed.status == ConfigurationCommitStatus::NoChange;
    }

    static bool coreReady(const FermentationApplication& application) {
        return application.applicationReadiness();
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;

CrossRolePlausibilityContext uiEvidence() {
    CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

class MockHttpServerLifecycle final
    : public device_platform::IHttpServerLifecycle {
   public:
    [[nodiscard]] bool start(device_platform::IHttpRouteSink&) override {
        ++startCallCount_;
        if (!startResult_) {
            running_ = false;
            return false;
        }
        running_ = true;
        return true;
    }

    [[nodiscard]] bool stop() override {
        running_ = false;
        return true;
    }

    [[nodiscard]] bool running() const override { return running_; }

    void setStartResult(bool result) noexcept { startResult_ = result; }

    [[nodiscard]] std::size_t startCallCount() const noexcept {
        return startCallCount_;
    }

   private:
    bool running_{false};
    bool startResult_{true};
    std::size_t startCallCount_{0U};
};

void seedApOnlyConfiguration(
    device_platform_test_support::SimulatedPersistentStateStore& store,
    device_platform_test_support::MockTimeZoneResolver& timeZoneResolver) {
    device_platform::DevicePlatform platform;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                       timeSource, network, http,
                                       randomSource));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(
            application.applyNetworkMode(device_platform::NetworkMode::AP_ONLY)
                .status));
}

void seedHomeWifiSetupConfiguration(
    device_platform_test_support::SimulatedPersistentStateStore& store,
    device_platform_test_support::MockTimeZoneResolver& timeZoneResolver) {
    seedApOnlyConfiguration(store, timeZoneResolver);

    device_platform::DevicePlatform platform;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                       timeSource, network, http,
                                       randomSource));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(
            application
                .applyNetworkMode(device_platform::NetworkMode::HOME_WIFI)
                .status));
}

RunCommandState standbyState() {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    return state;
}

FermentationUiCommandContext context(const RunCommandState& state,
                                     bool confirmed = false) {
    FermentationUiCommandContext value;
    value.surface = device_platform::UiSurface::LocalDisplay;
    value.monotonicMillis = 100U;
    value.expected.expectedStateSequence =
        state.processState.transitionSequence;
    value.expected.expectedRunRevision = state.runRevision;
    value.expected.expectedMessageRevision = state.messageRevision;
    value.expected.expectedFaultRevision = state.faultRevision;
    value.confirmed = confirmed;
    return value;
}

std::optional<FermentationUiConfirmationRequest> confirmation(
    const FermentationUiCommandContext& value) {
    return FermentationUiCommandBridge::confirmationRequest(
        value, FermentationUiAction::StartProgram,
        {device_platform::TextNamespace{"fermentation"},
         "ui.command.start.title"},
        {device_platform::TextNamespace{"fermentation"},
         "ui.command.start.summary"});
}

CommandStatus commandDetail(const FermentationUiCommandResult& result) {
    TEST_ASSERT_TRUE(std::holds_alternative<CommandStatus>(result.detail));
    return std::get<CommandStatus>(result.detail);
}

void assertDecisionOnly(const FermentationUiCommandResult& result) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::DecisionOnly),
        static_cast<int>(result.phase));
}

void test_ui_request_id_is_the_existing_command_id() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    application.publishOwningRuntimeEvidence(uiEvidence());
    FermentationUiStartManualHoldingIntent manual;
    manual.plan.targetTemperatureCelsius = 30.0;
    manual.plan.qualificationBandCelsius = 0.5;
    manual.plan.qualificationDurationMinutes = 10U;
    manual.plan.maximumTargetReachMinutes = 60U;
    FermentationUiCommandContext value;
    value.surface = device_platform::UiSurface::WebInterface;
    value.monotonicMillis = 100U;
    const auto prepared =
        FermentationApplicationTestAccess::prepareStartManualHoldingBody(
            application, value, manual);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_TRUE(prepared.uiRequestId.has_value());
    TEST_ASSERT_EQUAL_UINT64(prepared.uiRequestId->value,
                             prepared.request->commandEnvelope().id);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandSource::WebInterface),
        static_cast<int>(prepared.request->commandEnvelope().source));
    TEST_ASSERT_FALSE(prepared.request->commandEnvelope().confirmed);
}

void test_canonical_validation_precedes_ui_confirmation() {
    const auto state = standbyState();
    const auto unconfirmedContext = context(state, false);
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    application.publishOwningRuntimeEvidence(uiEvidence());
    FermentationUiStartManualHoldingIntent manual;
    manual.plan.targetTemperatureCelsius = 30.0;
    manual.plan.qualificationBandCelsius = 0.5;
    manual.plan.qualificationDurationMinutes = 10U;
    manual.plan.maximumTargetReachMinutes = 60U;
    const auto unconfirmedPrepared =
        FermentationApplicationTestAccess::prepareStartManualHoldingBody(
            application, unconfirmedContext, manual);
    TEST_ASSERT_TRUE(unconfirmedPrepared.request.has_value());
    const auto unconfirmed = FermentationUiCommandBridge::decidePrepared(
        state, *unconfirmedPrepared.request, confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiCommandOutcomeCategory::
                             ConfirmationRequired),
        static_cast<int>(unconfirmed.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                          static_cast<int>(commandDetail(unconfirmed)));
    assertDecisionOnly(unconfirmed);
    TEST_ASSERT_TRUE(unconfirmed.confirmation.has_value());

    auto staleContext = unconfirmedContext;
    staleContext.expected.expectedRunRevision = 1U;
    const auto stalePrepared =
        FermentationApplicationTestAccess::prepareStartManualHoldingBody(
            application, staleContext, manual);
    TEST_ASSERT_TRUE(stalePrepared.request.has_value());
    const auto stale = FermentationUiCommandBridge::decidePrepared(
        state, *stalePrepared.request, confirmation(staleContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(stale.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::StaleState),
                          static_cast<int>(commandDetail(stale)));
    assertDecisionOnly(stale);
    TEST_ASSERT_FALSE(stale.confirmation.has_value());

    auto invalidManual = manual;
    invalidManual.plan.targetTemperatureCelsius = 0.0;
    const auto invalidPrepared =
        FermentationApplicationTestAccess::prepareStartManualHoldingBody(
            application, unconfirmedContext, invalidManual);
    TEST_ASSERT_TRUE(invalidPrepared.request.has_value());
    const auto invalidResult = FermentationUiCommandBridge::decidePrepared(
        state, *invalidPrepared.request, confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(invalidResult.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                          static_cast<int>(commandDetail(invalidResult)));
    assertDecisionOnly(invalidResult);
    TEST_ASSERT_FALSE(invalidResult.confirmation.has_value());

    application.publishOwningRuntimeEvidence(CrossRolePlausibilityContext{});
    const auto unsafePrepared =
        FermentationApplicationTestAccess::prepareStartManualHoldingBody(
            application, unconfirmedContext, manual);
    TEST_ASSERT_TRUE(unsafePrepared.request.has_value());
    const auto unsafeResult = FermentationUiCommandBridge::decidePrepared(
        state, *unsafePrepared.request, confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(unsafeResult.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                          static_cast<int>(commandDetail(unsafeResult)));
    assertDecisionOnly(unsafeResult);
    TEST_ASSERT_FALSE(unsafeResult.confirmation.has_value());
}

void test_command_result_preserves_typed_app_details() {
    const auto accepted = FermentationUiCommandBridge::fromCommandStatus(
        CommandStatus::AlreadyProcessed);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Accepted),
        static_cast<int>(accepted.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::AlreadyProcessed),
                          static_cast<int>(commandDetail(accepted)));

    const auto persisted =
        FermentationUiCommandBridge::fromRunPersistenceResult(
            RunPersistenceResultStatus::AlreadyPersisted);
    TEST_ASSERT_TRUE(
        std::holds_alternative<RunPersistenceResultStatus>(persisted.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::AlreadyPersisted),
        static_cast<int>(
            std::get<RunPersistenceResultStatus>(persisted.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(persisted.phase));

    const auto unsupported =
        FermentationUiCommandBridge::unsupportedAppDetail();
    assertDecisionOnly(unsupported);

    const auto preview = FermentationUiCommandBridge::fromConfigurationPreview(
        ConfigurationPreviewStatus::PreviewSuperseded);
    TEST_ASSERT_TRUE(
        std::holds_alternative<ConfigurationPreviewStatus>(preview.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::PreviewSuperseded),
        static_cast<int>(std::get<ConfigurationPreviewStatus>(preview.detail)));

    const auto commit = FermentationUiCommandBridge::fromConfigurationCommit(
        ConfigurationCommitStatus::ConfigurationMutationBusy);
    TEST_ASSERT_TRUE(
        std::holds_alternative<ConfigurationCommitStatus>(commit.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::ConfigurationMutationBusy),
        static_cast<int>(std::get<ConfigurationCommitStatus>(commit.detail)));
    assertDecisionOnly(commit);
}

void test_network_ui_commands_use_the_owning_application_paths() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                       timeSource, network, http,
                                       randomSource));

    FermentationUiCommand typedCommand;
    typedCommand.operation = FermentationUiApplyNetworkModeCommand{
        device_platform::NetworkMode::AP_ONLY};
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiApplyNetworkModeCommand>(
            typedCommand.operation));

    const auto apOnly = FermentationUiCommandBridge::applyNetworkMode(
        application, std::get<FermentationUiApplyNetworkModeCommand>(
                         typedCommand.operation));
    TEST_ASSERT_TRUE(
        std::holds_alternative<NetworkConfigurationStatus>(apOnly.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(std::get<NetworkConfigurationStatus>(apOnly.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(apOnly.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkMode::AP_ONLY),
        static_cast<int>(application.networkMode()));

    const auto accessPoint = application.networkAccessPointInfo();
    TEST_ASSERT_TRUE(accessPoint.has_value());
    TEST_ASSERT_EQUAL_UINT(16U, accessPoint->password.size());

    const auto homeWifi = FermentationUiCommandBridge::applyNetworkMode(
        application, FermentationUiApplyNetworkModeCommand{
                         device_platform::NetworkMode::HOME_WIFI});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(std::get<NetworkConfigurationStatus>(
                              homeWifi.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkMode::HOME_WIFI),
        static_cast<int>(application.networkMode()));

    FermentationUiCommand reconfigurationCommand;
    reconfigurationCommand.operation =
        FermentationUiBeginHomeWifiReconfigurationCommand{};
    TEST_ASSERT_TRUE(std::holds_alternative<
                     FermentationUiBeginHomeWifiReconfigurationCommand>(
        reconfigurationCommand.operation));
    const auto reconfigured =
        FermentationUiCommandBridge::beginHomeWifiReconfiguration(
            application,
            std::get<FermentationUiBeginHomeWifiReconfigurationCommand>(
                reconfigurationCommand.operation));
    TEST_ASSERT_TRUE(std::holds_alternative<NetworkConfigurationStatus>(
        reconfigured.detail));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(std::get<NetworkConfigurationStatus>(
                              reconfigured.detail)));
    TEST_ASSERT_TRUE(network.startCallCount() >= 3U);
}

void test_application_network_restarts_refresh_hostname_and_preserve_password() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                       timeSource, network, http,
                                       randomSource));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(
            application.applyNetworkMode(device_platform::NetworkMode::AP_ONLY)
                .status));
    const auto firstAccessPoint = application.networkAccessPointInfo();
    TEST_ASSERT_TRUE(firstAccessPoint.has_value());
    const auto firstPassword = firstAccessPoint->password;

    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::rename(application, "Device-B"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(
            application.applyNetworkMode(device_platform::NetworkMode::AP_ONLY)
                .status));
    const auto secondAccessPoint = application.networkAccessPointInfo();
    TEST_ASSERT_TRUE(secondAccessPoint.has_value());
    TEST_ASSERT_EQUAL_STRING("Device-B", secondAccessPoint->ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("device-b", network.hostname().c_str());
    TEST_ASSERT_TRUE(secondAccessPoint->password == firstPassword);

    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::rename(application, "Device-C"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(
            application
                .applyNetworkMode(device_platform::NetworkMode::HOME_WIFI)
                .status));
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::rename(application, "Device-D"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(application.beginHomeWifiReconfiguration().status));
    const auto reconfiguredAccessPoint = application.networkAccessPointInfo();
    TEST_ASSERT_TRUE(reconfiguredAccessPoint.has_value());
    TEST_ASSERT_EQUAL_STRING("Device-D", reconfiguredAccessPoint->ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("device-d", network.hostname().c_str());
    TEST_ASSERT_TRUE(reconfiguredAccessPoint->password == firstPassword);
}

void test_home_wifi_reconfiguration_restores_http_after_boot_failure() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    seedHomeWifiSetupConfiguration(store, timeZoneResolver);

    device_platform::DevicePlatform platform;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    http.setStartResult(false);
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                       timeSource, network, http,
                                       randomSource));
    TEST_ASSERT_TRUE(application.ready());
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::coreReady(application));
    TEST_ASSERT_FALSE(http.running());
    TEST_ASSERT_TRUE(network.status().state ==
                     device_platform::NetworkLifecycleState::Stopped);

    http.setStartResult(true);
    const auto recovered = application.beginHomeWifiReconfiguration();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(recovered.status));
    TEST_ASSERT_TRUE(http.running());
    TEST_ASSERT_TRUE(network.status().state !=
                     device_platform::NetworkLifecycleState::Stopped);

    TEST_ASSERT_TRUE(http.stop());
    http.setStartResult(false);
    const auto failedRetry = application.beginHomeWifiReconfiguration();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::TransportFailure),
        static_cast<int>(failedRetry.status));
    TEST_ASSERT_FALSE(http.running());
    TEST_ASSERT_TRUE(network.status().state ==
                     device_platform::NetworkLifecycleState::Stopped);
}

void test_application_network_failures_keep_core_ready_and_fail_closed() {
    {
        device_platform_test_support::SimulatedPersistentStateStore store;
        device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
        seedApOnlyConfiguration(store, timeZoneResolver);
        store.injectReadFailure(ConnectivityCredentialStore::key(), true);

        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        device_platform_test_support::MockNetworkLifecycle network;
        device_platform_test_support::MockSecureRandomSource randomSource;
        MockHttpServerLifecycle http;
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http,
                                           randomSource));
        TEST_ASSERT_TRUE(application.ready());
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::coreReady(application));
        TEST_ASSERT_TRUE(network.status().state ==
                         device_platform::NetworkLifecycleState::Stopped);
        TEST_ASSERT_FALSE(http.running());
    }

    {
        device_platform_test_support::SimulatedPersistentStateStore store;
        device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
        seedApOnlyConfiguration(store, timeZoneResolver);

        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        device_platform_test_support::MockNetworkLifecycle network;
        network.setStartStatus(device_platform::NetworkOperationStatus::Failed);
        device_platform_test_support::MockSecureRandomSource randomSource;
        MockHttpServerLifecycle http;
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http,
                                           randomSource));
        TEST_ASSERT_TRUE(application.ready());
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::coreReady(application));
        TEST_ASSERT_TRUE(network.status().state ==
                         device_platform::NetworkLifecycleState::Stopped);
        TEST_ASSERT_FALSE(http.running());
    }

    {
        device_platform_test_support::SimulatedPersistentStateStore store;
        device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
        seedApOnlyConfiguration(store, timeZoneResolver);

        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        device_platform_test_support::MockNetworkLifecycle network;
        device_platform_test_support::MockSecureRandomSource randomSource;
        MockHttpServerLifecycle http;
        http.setStartResult(false);
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http,
                                           randomSource));
        TEST_ASSERT_TRUE(application.ready());
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::coreReady(application));
        TEST_ASSERT_TRUE(network.status().state ==
                         device_platform::NetworkLifecycleState::Stopped);
        TEST_ASSERT_FALSE(http.running());
    }

    {
        device_platform_test_support::SimulatedPersistentStateStore store;
        device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
        seedApOnlyConfiguration(store, timeZoneResolver);

        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        device_platform_test_support::MockNetworkLifecycle network;
        device_platform_test_support::MockSecureRandomSource randomSource;
        MockHttpServerLifecycle http;
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http,
                                           randomSource));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(NetworkConfigurationStatus::Applied),
            static_cast<int>(
                application
                    .applyNetworkMode(device_platform::NetworkMode::HOME_WIFI)
                    .status));
        store.forceNotFound(ConnectivityCredentialStore::key(), true);
        store.setNextWriteFault(
            device_platform_test_support::SimulatedPersistentStateStore::
                WriteFault::FailBeforeBegin);
        const auto failedWrite = application.beginHomeWifiReconfiguration();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(NetworkConfigurationStatus::PersistenceFailure),
            static_cast<int>(failedWrite.status));
        TEST_ASSERT_TRUE(application.ready());
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::coreReady(application));
        TEST_ASSERT_TRUE(network.status().state ==
                         device_platform::NetworkLifecycleState::Stopped);
        TEST_ASSERT_FALSE(http.running());
    }

    {
        device_platform_test_support::SimulatedPersistentStateStore store;
        device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
        seedApOnlyConfiguration(store, timeZoneResolver);

        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        device_platform_test_support::MockNetworkLifecycle network;
        device_platform_test_support::MockSecureRandomSource randomSource;
        MockHttpServerLifecycle http;
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http,
                                           randomSource));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(NetworkConfigurationStatus::Applied),
            static_cast<int>(
                application
                    .applyNetworkMode(device_platform::NetworkMode::HOME_WIFI)
                    .status));
        store.forceNotFound(ConnectivityCredentialStore::key(), true);
        store.failNextReadAfterWrite();
        const auto failedReadback = application.beginHomeWifiReconfiguration();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(NetworkConfigurationStatus::CommitIndeterminate),
            static_cast<int>(failedReadback.status));
        TEST_ASSERT_TRUE(application.ready());
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::coreReady(application));
        TEST_ASSERT_TRUE(network.status().state ==
                         device_platform::NetworkLifecycleState::Stopped);
        TEST_ASSERT_FALSE(http.running());
    }
}

void test_unselected_network_command_is_rejected_without_a_third_ui_option() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                       timeSource, network, http,
                                       randomSource));

    const auto result = FermentationUiCommandBridge::applyNetworkMode(
        application, FermentationUiApplyNetworkModeCommand{
                         device_platform::NetworkMode::UNSELECTED});
    TEST_ASSERT_TRUE(
        std::holds_alternative<NetworkConfigurationStatus>(result.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::InvalidMode),
        static_cast<int>(std::get<NetworkConfigurationStatus>(result.detail)));
    TEST_ASSERT_EQUAL_INT(0U, network.startCallCount());
}

void test_ui_payloads_are_intents_and_not_owning_evidence() {
    static_assert(!std::is_constructible_v<FermentationUiEnvelopePayload,
                                           ProgramStartRequest>);
    static_assert(!std::is_constructible_v<FermentationUiEnvelopePayload,
                                           ManualStartRequest>);
    static_assert(!std::is_constructible_v<FermentationUiEnvelopePayload,
                                           SensorSelectionCommandRequest>);

    FermentationUiStartProgramIntent start;
    start.candidate.programId = "water-kefir";
    start.candidate.sensorMode = RunSensorMode::Product;
    FermentationUiEnvelopePayload payload = start;
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiStartProgramIntent>(payload));
    TEST_ASSERT_EQUAL_STRING("water-kefir",
                             std::get<FermentationUiStartProgramIntent>(payload)
                                 .candidate.programId.c_str());

    FermentationUiSensorSelectionIntent selection;
    selection.action = SensorSelectionUserAction::RecheckProduct;
    payload = selection;
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiSensorSelectionIntent>(payload));
}

void test_proposed_decision_is_not_reported_as_applied() {
    const auto proposed =
        FermentationUiCommandBridge::fromCommandStatus(CommandStatus::Proposed);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Accepted),
        static_cast<int>(proposed.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Proposed),
                          static_cast<int>(commandDetail(proposed)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::DecisionOnly),
        static_cast<int>(proposed.phase));
}

void test_manual_timed_ui_intent_uses_the_merged_application_contract() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    application.publishOwningRuntimeEvidence(uiEvidence());

    FermentationUiStartManualTimedIntent intent;
    intent.values.targetTemperatureCelsius = 30.0;
    intent.values.durationMinutes = 60U;
    intent.values.sensorMode = RunSensorMode::Air;
    intent.values.preheatEnabled = false;
    intent.values.qualificationBandCelsius = 0.5;
    intent.values.qualificationDurationMinutes = 10U;
    intent.values.maximumTargetReachMinutes = 180U;
    FermentationUiCommandContext value;
    value.expected.expectedStateSequence = 0U;
    // The public entries refuse it on every surface (O5).
    const auto refused = application.prepareEnvelope(
        value, FermentationUiEnvelopePayload{intent});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Unavailable),
        static_cast<int>(refused.status));
    TEST_ASSERT_FALSE(refused.request.has_value());

    const auto prepared =
        FermentationApplicationTestAccess::prepareStartManualTimedBody(
            application, value, intent.values);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(prepared.status));
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, prepared.request->commandId());
    TEST_ASSERT_TRUE(prepared.request->runId().has_value());

    intent.values.targetTemperatureCelsius = -100.0;
    const auto invalid =
        FermentationApplicationTestAccess::prepareStartManualTimedBody(
            application, value, intent.values);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::InvalidInput),
        static_cast<int>(invalid.status));
    TEST_ASSERT_FALSE(invalid.request.has_value());
}

void test_product_inserted_decision_uses_state_revision_without_apply() {
    RunCommandState state;
    state.processState.state = ProcessState::WaitingForProduct;
    ProcessRunSnapshot runSnapshot;
    runSnapshot.kind = ProcessKind::Timed;
    runSnapshot.preheatEnabled = true;
    runSnapshot.maximumProductWaitMinutes = 30U;
    runSnapshot.completionMode = CompletionMode::FinishWithoutCooling;
    runSnapshot.qualificationDurationMinutes = 10U;
    runSnapshot.maximumTargetReachMinutes = 60U;
    runSnapshot.fermentationDurationMinutes = 60U;
    FermentationUiCommandContext value;
    value.expected.expectedStateSequence = 0U;
    const auto proposed =
        FermentationUiCommandBridge::decideProductInsertedConfirmed(
            state, &runSnapshot, value, ProcessSignals{}, 100U);
    TEST_ASSERT_TRUE(std::holds_alternative<DecisionStatus>(proposed.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DecisionStatus::Proposed),
        static_cast<int>(std::get<DecisionStatus>(proposed.detail)));
    assertDecisionOnly(proposed);

    value.expected.expectedStateSequence = 1U;
    const auto stale =
        FermentationUiCommandBridge::decideProductInsertedConfirmed(
            state, &runSnapshot, value, ProcessSignals{}, 100U);
    TEST_ASSERT_TRUE(std::holds_alternative<CommandStatus>(stale.detail));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::StaleState),
                          static_cast<int>(commandDetail(stale)));
    assertDecisionOnly(stale);
}

void test_prepared_message_actions_remain_bound_to_their_action() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));

    auto state = standbyState();
    state.messageCount = 1U;
    state.messages[0].id = 7U;
    state.messages[0].acknowledged = true;
    state.messages[0].acousticMuted = false;

    FermentationUiCommandContext value = context(state, false);
    value.expected.expectedMessageRevision = state.messageRevision;
    const auto acknowledge = application.prepareEnvelope(
        value, FermentationUiEnvelopePayload{
                   FermentationUiAcknowledgeMessageIntent{7U}});
    const auto mute = application.prepareEnvelope(
        value,
        FermentationUiEnvelopePayload{FermentationUiMuteMessageIntent{7U}});
    TEST_ASSERT_TRUE(acknowledge.request.has_value());
    TEST_ASSERT_TRUE(mute.request.has_value());

    const auto confirmedAcknowledge = application.confirmPrepared(acknowledge);
    const auto confirmedMute = application.confirmPrepared(mute);
    const auto acknowledgeResult = FermentationUiCommandBridge::decidePrepared(
        state, *confirmedAcknowledge.request);
    const auto muteResult = FermentationUiCommandBridge::decidePrepared(
        state, *confirmedMute.request);

    // The acknowledged message makes Ack a NoChange, while the independent
    // acoustic flag makes Mute a Proposed decision. If the prepared payload
    // were reinterpreted by the bridge, these outcomes would be swapped.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NoChange),
                          static_cast<int>(commandDetail(acknowledgeResult)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Proposed),
                          static_cast<int>(commandDetail(muteResult)));
    assertDecisionOnly(acknowledgeResult);
    assertDecisionOnly(muteResult);
    TEST_ASSERT_NOT_EQUAL(acknowledge.request->commandId(),
                          mute.request->commandId());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ui_request_id_is_the_existing_command_id);
    RUN_TEST(test_canonical_validation_precedes_ui_confirmation);
    RUN_TEST(test_command_result_preserves_typed_app_details);
    RUN_TEST(test_network_ui_commands_use_the_owning_application_paths);
    RUN_TEST(
        test_application_network_restarts_refresh_hostname_and_preserve_password);
    RUN_TEST(test_home_wifi_reconfiguration_restores_http_after_boot_failure);
    RUN_TEST(test_application_network_failures_keep_core_ready_and_fail_closed);
    RUN_TEST(
        test_unselected_network_command_is_rejected_without_a_third_ui_option);
    RUN_TEST(test_ui_payloads_are_intents_and_not_owning_evidence);
    RUN_TEST(test_proposed_decision_is_not_reported_as_applied);
    RUN_TEST(test_manual_timed_ui_intent_uses_the_merged_application_contract);
    RUN_TEST(test_product_inserted_decision_uses_state_revision_without_apply);
    RUN_TEST(test_prepared_message_actions_remain_bound_to_their_action);
    return UNITY_END();
}
