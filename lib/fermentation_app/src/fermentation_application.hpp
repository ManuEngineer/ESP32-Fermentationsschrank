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
#include "authentication_records.hpp"
#include "fermentation_ui_commands.hpp"
#include "fermentation_ui_projector.hpp"
#include "http_server_lifecycle.hpp"
#include "connectivity_credentials.hpp"
#include "network_lifecycle.hpp"
#include "network_configuration_service.hpp"
#include "network_setup_routes.hpp"
#include "replay_digest.hpp"
#include "secure_random_source.hpp"

namespace fermentation {

class ConfigurationBootstrapStore;
class ConfigurationGraphStore;
class ConfigurationMutationCoordinator;
class ConfigurationRecoveryService;
struct ConfigurationRecoveryResult;
class ConfigurationService;
class RunPersistenceCoordinator;
class WebRouteDispatcher;
class WebSessionManager;
enum class ConfigurationRecoveryStatus : std::uint8_t;
class FermentationApplicationTestAccess;

#if defined(APP_ISSUE_90_SLICE7_HARNESS)
namespace issue_90_slice7 {
class Harness;
}
#endif

// One narrow, application-owned gate is shared by the main/touch loop and
// every Web callback. Recursive locking is intentional for existing owning
// paths that call nested public projections; PBKDF2 runs outside this gate.
class ApplicationCallSerializer final {
   public:
    class Guard final {
       public:
        ~Guard() = default;
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        Guard(Guard&&) noexcept = default;
        Guard& operator=(Guard&&) noexcept = default;

       private:
        friend class ApplicationCallSerializer;
        explicit Guard(std::recursive_mutex& mutex) : lock_(mutex) {}
        std::unique_lock<std::recursive_mutex> lock_;
    };

    [[nodiscard]] Guard enter() const { return Guard(mutex_); }

   private:
    mutable std::recursive_mutex mutex_;
};

enum class WebAuthenticationState : std::uint8_t {
    PasswordProtected,
    PasswordDisabled,
    Unprovisioned,
    RecoveryRequired,
    Indeterminate,
};

enum class WebAuthenticationResultStatus : std::uint8_t {
    Authenticated,
    Disabled,
    Invalid,
    LockedOut,
    RecoveryRequired,
    KdfUnavailable,
};

struct WebAuthenticationResult {
    WebAuthenticationResultStatus status{
        WebAuthenticationResultStatus::RecoveryRequired};
    std::uint64_t retryAfterMs{0U};
};

class FermentationApplication {
   public:
    FermentationApplication() noexcept;
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
        device_platform::ISecureRandomSource& randomSource,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::ITimeSource& timeSource,
        device_platform::INetworkLifecycle& networkLifecycle,
        device_platform::IHttpServerLifecycle& httpServerLifecycle,
        device_platform::ISecureRandomSource& randomSource,
        IAuthenticationKdf& authenticationKdf,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices,
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver,
        const device_platform::ITimeSource& timeSource,
        device_platform::INetworkLifecycle& networkLifecycle,
        device_platform::IHttpServerLifecycle& httpServerLifecycle,
        device_platform::ISecureRandomSource& randomSource,
        IAuthenticationKdf& authenticationKdf,
        device_platform::IReplayDigest& replayDigest,
        const device_platform::IResetCauseSource* resetCauseSource = nullptr);
    void update();
    [[nodiscard]] NetworkConfigurationResult applyNetworkMode(
        device_platform::NetworkMode selectedMode);
    [[nodiscard]] NetworkConfigurationResult beginHomeWifiReconfiguration();
    // Renderer-independent local setup data for the currently active
    // SoftAP. The caller owns display/QR rendering; HTTP routes never expose
    // these credentials.
    [[nodiscard]] std::optional<device_platform::NetworkAccessPointInfo>
    networkAccessPointInfo() const;
    [[nodiscard]] bool networkSetupFlowActive() const noexcept;
    // Secret-free canonical mode input for the renderer-independent UI view.
    [[nodiscard]] device_platform::NetworkMode networkMode() const noexcept;
    // Sole application-owned runtime evidence handoff. Producers such as
    // #30 may publish the already evaluated role snapshots here; UI, touch
    // and web adapters never provide runtime evidence.
    void publishOwningRuntimeEvidence(
        const CrossRolePlausibilityContext& evidence);
    [[nodiscard]] FermentationUiSnapshot uiSnapshot() const;
    // The single renderer-independent source for display locale, the
    // canonical prepared time zone and the program catalog needed by the
    // local UI workspace/renderer. It never duplicates persistence or
    // recovery policy; a failed configuration read yields safe defaults
    // (English, an empty catalog) rather than blocking presentation.
    [[nodiscard]] FermentationUiPresentationSource uiPresentationSource() const;

    [[nodiscard]] WebAuthenticationState webAuthenticationState() const;
    [[nodiscard]] WebAuthenticationResult authenticateWebPassword(
        const std::string& password, std::uint64_t nowMs);

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
    // The single confirmed envelope handoff for local and future UI
    // adapters. It reuses decidePrepared() and then enters the existing
    // application-owned apply/persistence path without allocating a new
    // identity or carrying UI-side evidence into the owner.
    [[nodiscard]] FermentationUiCommandResult applyConfirmedPrepared(
        const FermentationApplicationPreparedRequest& confirmed);

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
    [[nodiscard]] WebAuthenticationState webAuthenticationStateUnlocked() const;
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
        IAuthenticationKdf* authenticationKdf = nullptr,
        device_platform::IReplayDigest* replayDigest = nullptr);
    [[nodiscard]] bool initializeNetwork(
        device_platform::IStateStore& store,
        device_platform::StorageEpoch storageEpoch,
        device_platform::NetworkMode selectedMode,
        const std::string& canonicalDeviceName,
        device_platform::ISecureRandomSource* randomSource,
        device_platform::IReplayDigest* replayDigest);
    void resetAuthenticationState() noexcept;
    void initializeAuthentication(device_platform::IStateStore& store);
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
    std::unique_ptr<WebSessionManager> webSessionManager_;
    std::unique_ptr<WebRouteDispatcher> webRouteDispatcher_;
    std::unique_ptr<RunPersistenceCoordinator> runPersistenceCoordinator_;
    std::unique_ptr<ApplicationRunIdentity> runIdentity_;
    device_platform::IStateStore* stateStore_{nullptr};
    device_platform::INetworkLifecycle* networkLifecycle_{nullptr};
    device_platform::IHttpServerLifecycle* httpServerLifecycle_{nullptr};
    device_platform::ISecureRandomSource* secureRandomSource_{nullptr};
    IAuthenticationKdf* authenticationKdf_{nullptr};
    std::unique_ptr<AuthenticationRecordStore> authenticationRecordStore_;
    std::unique_ptr<AuthenticationDomain> authenticationDomain_;
    std::optional<AuthenticationBootstrapContext> authenticationContext_;
    AuthenticationBootstrapResolutionStatus authenticationResolutionStatus_{
        AuthenticationBootstrapResolutionStatus::RecoveryRequired};
    mutable AuthBootstrapStatus authenticationBootstrapStatus_{
        AuthBootstrapStatus::RecoveryRequired};
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
    ApplicationCallSerializer applicationCallSerializer_;
};

}  // namespace fermentation
