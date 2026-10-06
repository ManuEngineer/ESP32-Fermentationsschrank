#pragma once

#include <cstdint>
#include <condition_variable>
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
#include "web_session.hpp"

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

// Lifetime gate for slow authentication operations (PBKDF2 runs outside the
// ApplicationCallSerializer). A running operation holds a Token; reset,
// re-initialisation and destruction close the gate and drain active tokens
// before the domain/record store are destroyed or the storage epoch changes.
// Lock order: ApplicationCallSerializer (L1) -> gate mutex (L2). The gate
// mutex is only held briefly, never during a domain/store operation or KDF,
// and L1 is never acquired while it is held. The std::mutex /
// std::condition_variable allocate small pthread objects on ESP-IDF.
class AuthOperationGate final {
   public:
    class Token final {
       public:
        Token(const Token&) = delete;
        Token& operator=(const Token&) = delete;
        Token(Token&& other) noexcept : gate_(other.gate_) {
            other.gate_ = nullptr;
        }
        Token& operator=(Token&& other) noexcept {
            if (this != &other) {
                if (gate_ != nullptr) {
                    gate_->end();
                }
                gate_ = other.gate_;
                other.gate_ = nullptr;
            }
            return *this;
        }
        ~Token() {
            if (gate_ != nullptr) {
                gate_->end();
            }
        }

       private:
        friend class AuthOperationGate;
        explicit Token(AuthOperationGate& gate) noexcept : gate_(&gate) {}
        AuthOperationGate* gate_;
    };

    AuthOperationGate() = default;
    ~AuthOperationGate() = default;
    AuthOperationGate(const AuthOperationGate&) = delete;
    AuthOperationGate& operator=(const AuthOperationGate&) = delete;
    AuthOperationGate(AuthOperationGate&&) = delete;
    AuthOperationGate& operator=(AuthOperationGate&&) = delete;

    // Empty while the gate is closed.
    [[nodiscard]] std::optional<Token> tryBegin() {
        const std::scoped_lock lock(mutex_);
        if (closed_) {
            return std::nullopt;
        }
        ++active_;
        return Token(*this);
    }
    void closeAndDrain() {
        std::unique_lock<std::mutex> lock(mutex_);
        closed_ = true;
        drained_.wait(lock, [this] { return active_ == 0U; });
    }
    void reopen() {
        const std::scoped_lock lock(mutex_);
        closed_ = false;
    }

   private:
    friend class FermentationApplicationTestAccess;
    void end() noexcept {
        const std::scoped_lock lock(mutex_);
        if (--active_ == 0U) {
            drained_.notify_all();
        }
    }

    std::mutex mutex_;
    std::condition_variable drained_;
    unsigned active_{0U};
    bool closed_{false};
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
    // Web trust generation observed when the authentication decision was
    // taken; issueWebSession() refuses the decision once a trust boundary
    // (session revocation, auth reset) has been crossed since.
    std::uint64_t trustGeneration{0U};
};

// First-time web access setup (no web password yet). The caller (S4 HTTP
// adapter) maps these to the wire contract; the Application owns the
// authorization (local release window) and the lifetime/trust contracts.
enum class WebProvisionMode : std::uint8_t {
    Protect,
    Disable,
};

enum class WebProvisionStatus : std::uint8_t {
    Provisioned,
    NotAllowed,
    AlreadyProvisioned,
    InvalidCredentials,
    RecoveryRequired,
    Failed,
};

inline constexpr std::uint64_t kWebProvisioningWindowMs = 600000U;

enum class WebSessionIssueStatus : std::uint8_t {
    Created,
    // The authentication decision is stale: a trust boundary was crossed or
    // the authentication state changed after it was taken. Fail closed.
    TrustBoundaryChanged,
    Unavailable,
};

struct WebSessionIssueResult {
    WebSessionIssueStatus status{WebSessionIssueStatus::Unavailable};
    WebSessionResult session;
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
    // Change identity of networkAccessPointInfo() without copying the
    // secret-bearing strings (see INetworkLifecycle::accessPointInfoRevision).
    [[nodiscard]] std::uint64_t networkAccessPointRevision() const noexcept;
    // Secret-free canonical mode input for the renderer-independent UI view.
    [[nodiscard]] device_platform::NetworkMode networkMode() const noexcept;
    // Sole application-owned runtime evidence handoff. Producers such as
    // #30 may publish the already evaluated role snapshots here; UI, touch
    // and web adapters never provide runtime evidence.
    void publishOwningRuntimeEvidence(
        const CrossRolePlausibilityContext& evidence);
    [[nodiscard]] FermentationUiSnapshot uiSnapshot() const;
    // Same snapshot into an existing object (recycled buffers): allocation-free
    // once the buffers have reached their capacity; used by the steady-state
    // UI loop. Not safe for concurrent callers.
    void refreshUiSnapshot(FermentationUiSnapshot& snapshot) const;
    // The single renderer-independent source for display locale, the
    // canonical prepared time zone and the program catalog needed by the
    // local UI workspace/renderer. It never duplicates persistence or
    // recovery policy. The result is explicit: a value only if the
    // configuration runtime granted its read lease, std::nullopt otherwise
    // (no configuration service, lease unavailable). Callers decide what an
    // unavailable source means; there is no heuristic over empty catalogs or
    // default values.
    [[nodiscard]] std::optional<FermentationUiPresentationSource>
    uiPresentationSource() const;

    [[nodiscard]] WebAuthenticationState webAuthenticationState() const;
    [[nodiscard]] WebAuthenticationResult authenticateWebPassword(
        const std::string& password, std::uint64_t nowMs);
    // Creates the browser session for a successful (or disabled-mode)
    // authentication decision. The recheck of the current authentication
    // state and trust generation and the session creation happen under the
    // Application gate, so a trust-boundary revocation cannot interleave.
    [[nodiscard]] WebSessionIssueResult issueWebSession(
        const WebAuthenticationResult& authentication, std::uint64_t nowMs);
    // Domain validates password and Service-PIN. Runs the slow KDF under the
    // AuthOperationGate (outside the Application gate) and only reports
    // Provisioned if no trust boundary was crossed meanwhile.
    [[nodiscard]] WebProvisionStatus provisionWebAccess(
        WebProvisionMode mode, const std::string& webPassword,
        const std::string& servicePin);
    // Opens the volatile local release window (fixed 10 minutes, not
    // extended). Returns true only if it was newly opened. Called by the
    // local touch action only.
    [[nodiscard]] bool openWebProvisioningWindow();
    // Read-only view of the release state for the local UI projection. It
    // uses the cached authentication state and never queries the domain, so
    // a running KDF cannot stall the UI loop; the press path
    // (openWebProvisioningWindow) re-validates authoritatively.
    [[nodiscard]] FermentationWebAccessState webAccessState() const;

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
    // Single place that revokes all browser sessions at a trust boundary and
    // advances the trust generation (callers hold the Application gate).
    void revokeWebSessionsAtTrustBoundary() noexcept;
    // Volatile local release window for the web first-time setup. Elapsed
    // time is compared (no deadline arithmetic); a backward or missing clock
    // closes the window.
    void closeWebProvisioningWindow() noexcept;
    [[nodiscard]] bool webProvisioningWindowOpenUnlocked() noexcept;
    [[nodiscard]] bool webProvisioningWindowStillOpenUnlocked() const noexcept;
    [[nodiscard]] static WebProvisionStatus projectProvisionResult(
        AuthBootstrapStatus bootstrapResult,
        AuthBootstrapStatus reinspected) noexcept;
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
    AuthOperationGate authOperationGate_;
    std::uint64_t webTrustGeneration_{0U};
    bool webProvisioningWindowOpen_{false};
    std::uint64_t webProvisioningWindowOpenedAtMs_{0U};
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
    mutable FermentationUiProjectionInput uiProjectionInput_;
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
