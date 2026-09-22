#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "boot_classification.hpp"
#include "process_state_machine.hpp"
#include "platform_services.hpp"
#include "presentation_state.hpp"
#include "reset_cause.hpp"
#include "state_store.hpp"
#include "sensor_selection.hpp"
#include "time_source.hpp"
#include "time_zone_resolver.hpp"
#include "application_run_identity.hpp"
#include "application_lifecycle.hpp"
#include "fermentation_ui_commands.hpp"
#include "fermentation_ui_projector.hpp"
#include "http_server_lifecycle.hpp"
#include "secure_random_source.hpp"
#include "connectivity_credentials.hpp"
#include "network_lifecycle.hpp"
#include "network_configuration_service.hpp"
#include "network_setup_routes.hpp"

namespace fermentation {

class ConfigurationBootstrapStore;
class ConfigurationGraphStore;
class ConfigurationMutationCoordinator;
class ConfigurationRecoveryService;
struct ConfigurationRecoveryResult;
class ConfigurationService;
class AuthenticationRecordStore;
class AuthenticationDomain;
class WebSessionManager;
class WebApplicationRoutes;
class WebRouteDispatcher;
class IAuthenticationKdf;
enum class AuthBootstrapStatus : std::uint8_t;
class RunPersistenceCoordinator;
struct RunPersistenceResult;
enum class ConfigurationRecoveryStatus : std::uint8_t;
class FermentationApplicationTestAccess;

#if defined(APP_ISSUE_90_SLICE7_HARNESS)
namespace issue_90_slice7 {
class Harness;
}
#endif

class FermentationApplication {
   public:
    FermentationApplication();
    FermentationApplication(const FermentationApplication&) = delete;
    FermentationApplication& operator=(const FermentationApplication&) = delete;
    FermentationApplication(FermentationApplication&&) = delete;
    FermentationApplication& operator=(FermentationApplication&&) = delete;
    ~FermentationApplication();

    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::ITimeSource& timeSource,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::ITimeSource& timeSource,
        device_platform::INetworkLifecycle& networkLifecycle,
        device_platform::IHttpServerLifecycle& httpServerLifecycle,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr,
        device_platform::ISecureRandomSource* randomSource = nullptr,
        IAuthenticationKdf* authenticationKdf = nullptr);
    void update();
    [[nodiscard]] NetworkConfigurationResult applyNetworkMode(
        device_platform::NetworkMode selectedMode,
        std::optional<UserConfigurationRevision> expectedRevision =
            std::nullopt,
        ChangeOriginKind origin = ChangeOriginKind::LocalDisplay);
    [[nodiscard]] NetworkConfigurationResult beginHomeWifiReconfiguration();
    [[nodiscard]] AuthBootstrapStatus bootstrapAuthentication(
        const FermentationUiCommandContext& context,
        const FermentationUiBootstrapAuthenticationCommand& command);
    // Renderer-independent local setup data for the currently active
    // SoftAP. The caller owns display/QR rendering; HTTP routes never expose
    // these credentials.
    [[nodiscard]] std::optional<device_platform::NetworkAccessPointInfo>
    networkAccessPointInfo() const;
    // Secret-free canonical mode input for the renderer-independent UI view.
    [[nodiscard]] device_platform::NetworkMode networkMode() const noexcept;
    // Sole application-owned runtime evidence handoff. Producers such as
    // #30 may publish the already evaluated role snapshots here; UI, touch
    // and web adapters never provide runtime evidence.
    void publishOwningRuntimeEvidence(
        const CrossRolePlausibilityContext& evidence);
    [[nodiscard]] FermentationUiSnapshot uiSnapshot() const;
    [[nodiscard]] std::optional<device_platform::StorageEpoch>
    currentStorageEpoch() const noexcept;

    [[nodiscard]] bool ready() const;
    [[nodiscard]] ApplicationLifecycleState lifecycleState() const noexcept {
        return lifecycleState_;
    }
    [[nodiscard]] std::optional<ProcessRuntimeState> publishedProcessState()
        const;
    [[nodiscard]] const PresentationState& presentationState() const noexcept {
        return presentationState_;
    }
    [[nodiscard]] std::optional<RecoveryDisposition> recoveryDisposition()
        const noexcept {
        return recoveryDisposition_;
    }

    // These composition points are shared by local and future Web adapters.
    // They allocate identity, resolve current configuration, and return an
    // already application-bound owning request. The adapter supplies values
    // and expected revisions only.
    [[nodiscard]] FermentationApplicationRequestResult prepareStartProgram(
        const FermentationUiCommandContext& context,
        const FermentationUiStartProgramIntent& intent);
    [[nodiscard]] FermentationApplicationRequestResult
    prepareStartManualHolding(
        const FermentationUiCommandContext& context,
        const FermentationUiStartManualHoldingIntent& intent);
    [[nodiscard]] FermentationApplicationRequestResult prepareStartManualTimed(
        const FermentationUiCommandContext& context,
        const ManualTimedRunValues& values);
    [[nodiscard]] FermentationApplicationRequestResult prepareStop(
        const FermentationUiCommandContext& context,
        const FermentationUiStopRunIntent& intent);
    [[nodiscard]] FermentationApplicationRequestResult prepareCompletion(
        const FermentationUiCommandContext& context,
        const FermentationUiCompleteRunIntent& intent);
    [[nodiscard]] FermentationApplicationRequestResult prepareEnvelope(
        const FermentationUiCommandContext& context,
        const FermentationUiEnvelopePayload& payload);
    // Confirmation reuses the already application-bound request.  It only
    // changes the existing envelope confirmation bit; it never allocates a
    // new CommandId or derives a replacement runId.
    [[nodiscard]] FermentationApplicationRequestResult confirmPrepared(
        const FermentationApplicationRequestResult& prepared);
    [[nodiscard]] RunPersistenceResult applyPreparedRequest(
        const FermentationApplicationPreparedRequest& request);

    // Existing configuration recovery remains the authorization owner. This
    // application entry point composes its FactoryResetCompleted result with
    // the run-persistence epoch handoff before publishing the new runtime.
    [[nodiscard]] ConfigurationRecoveryResult beginAuthorizedFactoryReset();

    // Explicit R1 selected-fallback action.  The command carries only the
    // app-owned confirmation/revision contract; fresh sensor/planner evidence
    // is supplied by the owning application/orchestrator boundary, never by
    // a UI transport.
    // The orchestrator publishes fresh owning evidence at the application
    // boundary. UI/Web commands never carry sensor, planner or safety data.
    void publishOwningRecoveryEvidence(
        const CrossRolePlausibilityContext& evidence);
    [[nodiscard]] RunPersistenceResult resumeFallback(
        const FermentationUiResumeFallbackCommand& command);

   private:
    struct ApplicationRuntimeEvidence {
        CrossRolePlausibilityContext plausibility;
        bool safetyAllowsStart{false};
        bool safetyAllowsCooling{false};
        bool airSensorValid{false};
        bool coolingSensorValid{false};
        bool productSensorValid{false};
    };

    [[nodiscard]] ApplicationRuntimeEvidence resolveRuntimeEvidence() const;
    [[nodiscard]] bool applicationReadiness() const;
    [[nodiscard]] static bool validSensor(
        const device_platform::SensorQualitySnapshot& snapshot) noexcept;
    [[nodiscard]] bool revalidatePreparedRequest(
        FermentationApplicationPreparedRequest& request);
    template <typename Request>
    [[nodiscard]] FermentationApplicationRequestResult makePreparedRequest(
        Request request,
        std::optional<CrossRolePlausibilityContext> owningPlausibility =
            std::nullopt);
    template <typename Intent>
    [[nodiscard]] FermentationApplicationRequestResult
    prepareAdditionalEnvelope(const FermentationUiCommandContext& context,
                              const Intent& intent);
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    friend class issue_90_slice7::Harness;
#endif
    friend class FermentationApplicationTestAccess;
    void requireService(FaultCode faultCode,
                        bool applicationAllocationFailure = false) noexcept;
    [[nodiscard]] bool publishStandby();
    [[nodiscard]] bool beginPersistent(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::ITimeSource* timeSource,
        const device_platform::IResetCauseSource* resetCauseSource,
        device_platform::INetworkLifecycle* networkLifecycle = nullptr,
        device_platform::IHttpServerLifecycle* httpServerLifecycle = nullptr,
        device_platform::ISecureRandomSource* randomSource = nullptr,
        IAuthenticationKdf* authenticationKdf = nullptr);
    [[nodiscard]] bool initializeNetwork(
        device_platform::IStateStore& store,
        device_platform::StorageEpoch storageEpoch,
        device_platform::NetworkMode selectedMode,
        const std::string& canonicalDeviceName,
        bool positiveAuthenticationEpochEvidence);
    [[nodiscard]] bool processBootClassification(
        BootClassification classification,
        const RunPersistenceSnapshot* snapshot,
        const RunCheckpointTime& bootTime);
    [[nodiscard]] bool prepareResumeOffer(
        const RunPersistenceSnapshot* snapshot);
    [[nodiscard]] bool prepareFallbackSelection(
        const RunPersistenceSnapshot* snapshot);
    [[nodiscard]] bool evaluateCurrentRecovery(
        const RunPersistenceSnapshot* snapshot,
        const RunCheckpointTime& bootTime);
    [[nodiscard]] bool processTerminalClassification(
        BootClassification classification,
        const RunPersistenceSnapshot* snapshot,
        const RunCheckpointTime& bootTime);
    [[nodiscard]] RunCheckpointTime currentCheckpointTime() const noexcept;
    [[nodiscard]] bool enterRecoveryEvaluationRamState(
        const RunCommandState& source);
    void reevaluateWaitingForTrustedTime();

    device_platform::IPlatformServices* platformServices_{nullptr};
    const device_platform::ITimeSource* timeSource_{nullptr};
    std::unique_ptr<ConfigurationBootstrapStore> bootstrapStore_;
    std::unique_ptr<ConfigurationMutationCoordinator> mutationCoordinator_;
    std::unique_ptr<ConfigurationGraphStore> graphStore_;
    std::unique_ptr<ConfigurationService> configurationService_;
    std::unique_ptr<ConfigurationRecoveryService> configurationRecoveryService_;
    std::unique_ptr<ConnectivityCredentialStore> connectivityCredentialStore_;
    std::unique_ptr<NetworkConfigurationService> networkConfigurationService_;
    std::unique_ptr<NetworkSetupRoutes> networkSetupRoutes_;
    std::unique_ptr<RunPersistenceCoordinator> runPersistenceCoordinator_;
    std::unique_ptr<ApplicationRunIdentity> runIdentity_;
    device_platform::IStateStore* stateStore_{nullptr};
    device_platform::INetworkLifecycle* networkLifecycle_{nullptr};
    device_platform::IHttpServerLifecycle* httpServerLifecycle_{nullptr};
    device_platform::ISecureRandomSource* randomSource_{nullptr};
    IAuthenticationKdf* authenticationKdf_{nullptr};
    std::unique_ptr<AuthenticationRecordStore> authenticationStore_;
    std::unique_ptr<AuthenticationDomain> authenticationDomain_;
    std::unique_ptr<WebSessionManager> webSessionManager_;
    std::unique_ptr<WebApplicationRoutes> webApplicationRoutes_;
    std::unique_ptr<WebRouteDispatcher> webRouteDispatcher_;
    std::optional<device_platform::StorageEpoch> storageEpoch_;
    std::unique_ptr<RunCommandState> runtimeRunState_;
    std::unique_ptr<RunCommandState> pendingResume_;
    std::unique_ptr<RunCommandState> pendingFallbackResume_;
    std::unique_ptr<RunCommandState> pendingRecoverySource_;
    std::optional<CrossRolePlausibilityContext> owningRecoveryEvidence_;
    CrossRolePlausibilityContext owningRuntimeEvidence_{};
    mutable FermentationUiRefreshRevisionTracker uiRefreshTracker_;
    std::optional<RunPersistenceLoadStatus> persistenceLoadStatus_;
    RunLoadDisposition loadDisposition_{RunLoadDisposition::SafeBoot};
    std::optional<RecoveryDisposition> recoveryDisposition_;
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    std::optional<ConfigurationRecoveryStatus> configurationRecoveryStatus_;
#endif
    ApplicationLifecycleState lifecycleState_{
        ApplicationLifecycleState::Initializing};
    PresentationState presentationState_;
    mutable std::recursive_mutex stateMutex_;
};

}  // namespace fermentation
