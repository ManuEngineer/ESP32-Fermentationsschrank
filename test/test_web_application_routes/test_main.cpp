#include <unity.h>

#include <array>
#include <memory>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <type_traits>
#include <variant>

#include "device_platform.hpp"
#include "configuration_limits.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_storage_contract.hpp"
#include "connectivity_credentials.hpp"
#include "fermentation_application.hpp"
#include "mock_time_zone_resolver.hpp"
#include "mock_network_lifecycle.hpp"
#include "program_limits.hpp"
#include "mock_secure_random_source.hpp"
#include "simulated_persistent_state_store.hpp"
#include "standard_program_catalog.hpp"
#include "virtual_time_source.hpp"
#include "web_application_routes.hpp"
#include "web_json_codec.hpp"

#include "../common/allocation_window.hpp"

namespace fermentation {

struct WebSessionManagerTestAccess {
    static bool hasActive(const WebSessionManager& manager) {
        for (const auto& session : manager.sessions_) {
            if (session.active) return true;
        }
        return false;
    }
};

class FermentationApplicationTestAccess {
   public:
    static std::size_t webCalls(const FermentationApplication& app) {
        return app.webConfigurationCallsForTest_;
    }
    static ConfigurationManifestReference manifest(
        const FermentationApplication& app) {
        const auto read = app.configurationService_->acquireRuntime();
        TEST_ASSERT_TRUE(read.lease.valid());
        return read.lease.get().manifestReference();
    }
    static RunCommandState& runtimeState(FermentationApplication& application) {
        return *application.runtimeRunState_;
    }

    static ApplicationCallSerializer::Guard enter(
        FermentationApplication& application) {
        return application.applicationCallSerializer_.enter();
    }

    // True when the session manager still holds any active session.
    static bool hasActiveSession(FermentationApplication& application,
                                 std::uint64_t /*nowMs*/) {
        return application.webSessionManager_ != nullptr &&
               WebSessionManagerTestAccess::hasActive(
                   *application.webSessionManager_);
    }

    // Moves an Unprovisioned root to RecoveryRequired through the legal
    // Provisioning step (test only; mirrors a failed bootstrap).
    static bool forceRootState(FermentationApplication& application,
                               AuthProvisioningState state) {
        if (state != AuthProvisioningState::RecoveryRequired ||
            application.stateStore_ == nullptr ||
            !application.authenticationContext_.has_value()) {
            return false;
        }
        AuthenticationRecordStore store(*application.stateStore_);
        const auto read =
            store.readRoot(application.authenticationContext_->storageEpoch());
        if (!read.value.has_value()) return false;
        auto provisioning = *read.value;
        provisioning.state = AuthProvisioningState::Provisioning;
        ++provisioning.recordSequence;
        if (store.writeRoot(*read.value, provisioning) !=
            AuthenticationWriteStatus::Success) {
            return false;
        }
        auto recovery = provisioning;
        recovery.state = AuthProvisioningState::RecoveryRequired;
        ++recovery.recordSequence;
        return store.writeRoot(provisioning, recovery) ==
               AuthenticationWriteStatus::Success;
    }

    static bool windowFlagOpen(const FermentationApplication& application) {
        return application.webProvisioningWindowOpen_;
    }
    static std::uint64_t trustGeneration(
        const FermentationApplication& application) {
        return application.webTrustGeneration_;
    }
    static void setWindowOpenedAt(FermentationApplication& application,
                                  std::uint64_t openedAtMs) {
        application.webProvisioningWindowOpenedAtMs_ = openedAtMs;
    }
    static WebProvisionStatus projectProvision(
        AuthBootstrapStatus result, AuthBootstrapStatus reinspected) {
        return FermentationApplication::projectProvisionResult(result,
                                                               reinspected);
    }

    static bool authGateClosed(FermentationApplication& application) {
        const std::lock_guard<std::mutex> lock(
            application.authOperationGate_.mutex_);
        return application.authOperationGate_.closed_;
    }

    static void closeAuthGate(FermentationApplication& application) {
        application.authOperationGate_.closeAndDrain();
    }

    static void reopenAuthGate(FermentationApplication& application) {
        application.authOperationGate_.reopen();
    }

    // Issue #188 A: replaces the authentication domain within the same epoch
    // and bootstrap sequence (runtime re-initialisation path).
    static void reinitializeAuthentication(
        FermentationApplication& application) {
        const auto guard = application.applicationCallSerializer_.enter();
        application.initializeAuthentication(*application.stateStore_);
    }

    static std::pair<device_platform::StorageEpoch, std::uint64_t>
    authContextIdentity(FermentationApplication& application) {
        const auto guard = application.applicationCallSerializer_.enter();
        TEST_ASSERT_TRUE(application.authenticationContext_.has_value());
        return {application.authenticationContext_->storageEpoch(),
                application.authenticationContext_->bootstrapSequence()};
    }

    static bool provision(FermentationApplication& application,
                          const std::string& password,
                          const std::string& servicePin) {
        if (application.authenticationDomain_ == nullptr ||
            !application.authenticationContext_.has_value()) {
            return false;
        }
        return application.authenticationDomain_->bootstrap(
                   *application.authenticationContext_,
                   device_platform::UiSurface::LocalDisplay, true, password,
                   servicePin) == AuthBootstrapStatus::BootstrapAllowed;
    }

    static bool setWebPasswordEnabled(FermentationApplication& application,
                                      bool enabled) {
        if (application.stateStore_ == nullptr ||
            !application.authenticationContext_.has_value()) {
            return false;
        }
        AuthenticationRecordStore store(*application.stateStore_);
        const auto read = store.readCredentials(
            application.authenticationContext_->storageEpoch());
        if (!read.value.has_value()) return false;
        auto target = *read.value;
        target.webPasswordEnabled = enabled;
        ++target.recordSequence;
        return store.writeCredentials(*read.value, target) ==
               AuthenticationWriteStatus::Success;
    }

    static bool setAuthProvisioningState(FermentationApplication& application,
                                         AuthProvisioningState state) {
        if (application.stateStore_ == nullptr ||
            !application.authenticationContext_.has_value()) {
            return false;
        }
        AuthenticationRecordStore store(*application.stateStore_);
        const auto read =
            store.readRoot(application.authenticationContext_->storageEpoch());
        if (!read.value.has_value()) return false;
        if (state == AuthProvisioningState::RecoveryRequired &&
            read.value->state == AuthProvisioningState::Provisioned) {
            auto provisioning = *read.value;
            provisioning.state = AuthProvisioningState::Provisioning;
            ++provisioning.recordSequence;
            if (store.writeRoot(*read.value, provisioning) !=
                AuthenticationWriteStatus::Success) {
                return false;
            }
            auto recovery = provisioning;
            recovery.state = AuthProvisioningState::RecoveryRequired;
            ++recovery.recordSequence;
            return store.writeRoot(provisioning, recovery) ==
                   AuthenticationWriteStatus::Success;
        }
        return false;
    }
};

struct WebRunMutationHandlerTestAccess {
    static ReplayOutcomeCode projectConfig(
        const ApplicationConfigurationChangeResult& result) {
        return WebRunMutationHandler::projectConfigurationResult(result);
    }
    static ReplayOutcomeCode project(
        const FermentationUiCommandResult& result) {
        return WebRunMutationHandler::projectCommandResult(result);
    }

    static ReplayOutcomeCode projectRequest(
        FermentationApplicationRequestStatus status) {
        return WebRunMutationHandler::projectRequestStatus(status);
    }
};

}  // namespace fermentation

namespace fermentation {
struct WebRouteDispatcherTestAccess {
    static void extraAssetBytes(std::size_t bytes) {
        WebRouteDispatcher::extraAssetBytesForTest_ = bytes;
    }
    static void extraShellBytes(std::size_t bytes) {
        WebRouteDispatcher::extraShellBytesForTest_ = bytes;
    }
};
}  // namespace fermentation

#include "shell_golden.hpp"
#include "web_application_asset.hpp"
#include "crc32.hpp"

namespace {
std::string completeBody(const device_platform::HttpResponse& response) {
    return response.body + std::string(response.staticBodySuffix);
}

// Reassemble the actual two-script document only for existing semantic
// string/golden contracts. HTTP byte budgets are tested on the raw response.
std::string shellDocument(const device_platform::HttpResponse& response) {
    auto raw = completeBody(response);
    // The new navigation CSS is verified on the raw HTTP response below;
    // strip only that additive style when comparing the original frozen head.
    const std::string navCss =
        "nav{display:flex;flex-wrap:wrap;gap:.5rem}nav[hidden]{display:none}";
    if (const auto at = raw.find(navCss); at != std::string::npos)
        raw.erase(at, navCss.size());
    const auto split = raw.find(";</script><p id=asset-error");
    if (split == std::string::npos) return raw;
    return raw.substr(0U, split) + ";" + fermentation::kWebApplicationAsset +
           "</script></body></html>";
}

using namespace fermentation;
using Category = device_platform::DeviceUiCommandOutcomeCategory;

class DeterministicRandom final : public device_platform::ISecureRandomSource {
   public:
    bool fill(void* buffer, std::size_t length) override {
        if (length == 0U) return true;
        if (buffer == nullptr || fail) return false;
        auto* bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t index = 0U; index < length; ++index) {
            bytes[index] = next_++;
        }
        return true;
    }

    bool fail{false};

   private:
    std::uint8_t next_{1U};
};

class DeterministicReplayDigest final : public IReplayDigest {
   public:
    bool digest(const ReplayDigestInput& input, ReplayDigest& out) override {
        std::string material;
        material.append(input.method.data(), input.method.size());
        material.push_back('\0');
        material.append(input.path.data(), input.path.size());
        material.push_back('\0');
        material.append(input.bodyLength.data(), input.bodyLength.size());
        material.push_back('\0');
        material.append(input.body.data(), input.body.size());
        std::uint32_t state = 2166136261U;
        for (const auto byte : material) {
            state ^= static_cast<std::uint8_t>(byte);
            state *= 16777619U;
        }
        for (std::size_t index = 0U; index < out.size(); ++index) {
            state ^= static_cast<std::uint32_t>(index + 1U);
            state *= 16777619U;
            out[index] = static_cast<std::uint8_t>(state >> 24U);
        }
        return true;
    }
};

class DeterministicKdf final : public IAuthenticationKdf {
   public:
    bool derive(
        const std::string& secret, const AuthVerifier& parameters,
        std::array<std::uint8_t, kAuthenticationVerifierBytes>& out) override {
        std::uint32_t state = 2166136261U;
        for (const auto byte : secret) {
            state ^= static_cast<std::uint8_t>(byte);
            state *= 16777619U;
        }
        for (const auto byte : parameters.salt) {
            state ^= byte;
            state *= 16777619U;
        }
        for (std::size_t index = 0U; index < out.size(); ++index) {
            state ^= static_cast<std::uint32_t>(index + 1U);
            state *= 16777619U;
            out[index] = static_cast<std::uint8_t>(state >> 24U);
        }
        return true;
    }
};

// Delegates to the deterministic KDF; when armed, the next derivation
// signals that it is inside the slow operation and blocks until released.
class BlockableKdf final : public IAuthenticationKdf {
   public:
    bool derive(
        const std::string& secret, const AuthVerifier& parameters,
        std::array<std::uint8_t, kAuthenticationVerifierBytes>& out) override {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ++calls_;
            if (failing_) {
                return false;
            }
            if (armed_) {
                armed_ = false;
                entered_ = true;
                changed_.notify_all();
                changed_.wait(lock, [this] { return released_; });
            }
        }
        return inner_.derive(secret, parameters, out);
    }

    void arm() {
        const std::lock_guard<std::mutex> lock(mutex_);
        armed_ = true;
        entered_ = false;
        released_ = false;
    }
    [[nodiscard]] bool waitEntered() {
        std::unique_lock<std::mutex> lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(10),
                                 [this] { return entered_; });
    }
    void setFailing(bool failing) {
        const std::lock_guard<std::mutex> lock(mutex_);
        failing_ = failing;
    }
    void release() {
        const std::lock_guard<std::mutex> lock(mutex_);
        released_ = true;
        changed_.notify_all();
    }
    [[nodiscard]] unsigned calls() {
        const std::lock_guard<std::mutex> lock(mutex_);
        return calls_;
    }

   private:
    DeterministicKdf inner_;
    std::mutex mutex_;
    std::condition_variable changed_;
    unsigned calls_{0U};
    bool armed_{false};
    bool failing_{false};
    bool entered_{false};
    bool released_{false};
};

class CapturingHttpServerLifecycle final
    : public device_platform::IHttpServerLifecycle {
   public:
    [[nodiscard]] bool start(device_platform::IHttpRouteSink& routes) override {
        ++startCount_;
        routes_ = &routes;
        lastRoutes_ = &routes;
        running_ = true;
        return true;
    }

    [[nodiscard]] bool stop() override {
        ++stopCount_;
        running_ = false;
        routes_ = nullptr;
        return true;
    }

    [[nodiscard]] bool running() const override { return running_; }

    [[nodiscard]] device_platform::IHttpRouteSink* routes() const noexcept {
        return routes_;
    }

    [[nodiscard]] device_platform::IHttpRouteSink* lastRoutes() const noexcept {
        return lastRoutes_;
    }

    [[nodiscard]] std::size_t startCount() const noexcept {
        return startCount_;
    }

    [[nodiscard]] std::size_t stopCount() const noexcept { return stopCount_; }

   private:
    bool running_{false};
    std::size_t startCount_{0U};
    std::size_t stopCount_{0U};
    device_platform::IHttpRouteSink* routes_{nullptr};
    device_platform::IHttpRouteSink* lastRoutes_{nullptr};
};

struct ComposedFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    DeterministicRandom random;
    BlockableKdf kdf;
    DeterministicReplayDigest replayDigest;
    CapturingHttpServerLifecycle http;
    FermentationApplication application;

    explicit ComposedFixture(bool homeWifiConfigured = false) {
        TEST_ASSERT_TRUE(platform.begin({true}));
        ConnectivityCredentialStore credentials(store);
        std::optional<device_platform::NetworkCredentials> homeWifi;
        if (homeWifiConfigured) {
            homeWifi = device_platform::NetworkCredentials{
                "home-network", "home-network-password"};
        }
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ConnectivityCredentialWriteStatus::Committed),
            static_cast<int>(credentials
                                 .write({homeWifi, std::string(16U, 'A')},
                                        device_platform::StorageEpoch{1U}, 1U)
                                 .status));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http, random,
                                           kdf, replayDigest));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(NetworkConfigurationStatus::Applied),
            static_cast<int>(
                application
                    .applyNetworkMode(device_platform::NetworkMode::AP_ONLY)
                    .status));
        TEST_ASSERT_NOT_NULL(http.routes());
    }
};

device_platform::HttpRequest makeWebRequest(const std::string& method,
                                            const std::string& path,
                                            std::string body = {}) {
    device_platform::HttpRequest request;
    request.method = method;
    request.path = path;
    request.body = std::move(body);
    request.metadata.host = "fermenter.local";
    request.metadata.origin = "http://fermenter.local";
    request.metadata.secFetchSite = "same-origin";
    return request;
}

std::string sessionCookieValue(const device_platform::HttpResponse& response) {
    TEST_ASSERT_TRUE(response.metadata.setCookie.has_value());
    const std::string& cookie = *response.metadata.setCookie;
    constexpr char prefix[] = "FSSESSION=";
    TEST_ASSERT_TRUE(cookie.rfind(prefix, 0U) == 0U);
    const auto end = cookie.find(';');
    return cookie.substr(sizeof(prefix) - 1U,
                         end == std::string::npos
                             ? std::string::npos
                             : end - (sizeof(prefix) - 1U));
}

std::string csrfToken(const device_platform::HttpResponse& response) {
    constexpr char marker[] = "\"csrfToken\":\"";
    const auto start = completeBody(response).find(marker);
    TEST_ASSERT_TRUE(start != std::string::npos);
    const auto valueStart = start + sizeof(marker) - 1U;
    const auto end = completeBody(response).find('"', valueStart);
    TEST_ASSERT_TRUE(end != std::string::npos);
    return completeBody(response).substr(valueStart, end - valueStart);
}

device_platform::HttpRequest makeLoginRequest(const std::string& password) {
    auto request = makeWebRequest("POST", "/api/v1/login",
                                  "{\"password\":\"" + password + "\"}");
    request.metadata.contentType = "application/json; charset=utf-8";
    return request;
}

device_platform::HttpRequest makeAuthenticatedRequest(
    const std::string& method, const std::string& path,
    const std::string& cookie, const std::string& csrf = {}) {
    auto request = makeWebRequest(method, path);
    request.metadata.cookie = "FSSESSION=" + cookie;
    if (!csrf.empty()) request.metadata.csrfToken = csrf;
    return request;
}

device_platform::HttpRequest makeLogoutRequest(const std::string& cookie,
                                               const std::string& csrf) {
    auto request =
        makeAuthenticatedRequest("POST", "/api/v1/logout", cookie, csrf);
    request.metadata.contentType = "application/json; charset=utf-8";
    return request;
}

struct ComposedWebSession {
    std::string cookie;
    std::string csrf;
};

ComposedWebSession loginComposed(ComposedFixture& fixture) {
    device_platform::HttpResponse response;
    const auto request = makeLoginRequest("correct horse battery");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    return {sessionCookieValue(response), csrfToken(response)};
}

void assertComposedSessionRejected(ComposedFixture& fixture,
                                   const std::string& cookie) {
    auto request = makeAuthenticatedRequest("GET", "/api/v1/status", cookie);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);
}

void assertComposedSessionFailClosed(ComposedFixture& fixture,
                                     const std::string& cookie) {
    auto request = makeAuthenticatedRequest("GET", "/api/v1/status", cookie);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find(
                         "authentication-unavailable") != std::string::npos);
}

FermentationUiCommandResult commandResult(Category category,
                                          FermentationUiCommandDetail detail,
                                          FermentationUiCommandPhase phase) {
    return {category, std::move(detail), phase, std::nullopt, std::nullopt};
}

void assertProjected(const FermentationUiCommandResult& input,
                     std::uint16_t expectedStatus) {
    const auto projected =
        replayOutcome(WebRunMutationHandlerTestAccess::project(input));
    TEST_ASSERT_EQUAL_UINT16(expectedStatus, projected.statusCode);
    TEST_ASSERT_EQUAL_STRING("application/json; charset=utf-8",
                             projected.contentType.c_str());
}

void test_outcome_matrix_accepts_only_owning_apply_results() {
    for (const auto status : {RunPersistenceResultStatus::Applied,
                              RunPersistenceResultStatus::CheckpointWritten,
                              RunPersistenceResultStatus::AlreadyProcessed,
                              RunPersistenceResultStatus::AlreadyPersisted}) {
        assertProjected(
            commandResult(Category::Accepted, status,
                          FermentationUiCommandPhase::OwningOutcome),
            200U);
    }
    for (const auto status : {CommandStatus::Applied, CommandStatus::NoChange,
                              CommandStatus::AlreadyProcessed}) {
        assertProjected(
            commandResult(Category::Accepted, status,
                          FermentationUiCommandPhase::OwningOutcome),
            200U);
    }

    assertProjected(commandResult(Category::Accepted, CommandStatus::Proposed,
                                  FermentationUiCommandPhase::DecisionOnly),
                    503U);
    assertProjected(commandResult(Category::Accepted, CommandStatus::Applied,
                                  FermentationUiCommandPhase::DecisionOnly),
                    503U);
    assertProjected(
        commandResult(Category::Unavailable,
                      FermentationUiDetailStatus::UnsupportedAppDetail,
                      FermentationUiCommandPhase::OwningOutcome),
        503U);
}

void test_outcome_matrix_conflicts_busy_and_stale_are_409() {
    assertProjected(commandResult(Category::ConfirmationRequired,
                                  CommandStatus::NotConfirmed,
                                  FermentationUiCommandPhase::DecisionOnly),
                    409U);
    assertProjected(commandResult(Category::Rejected, CommandStatus::StaleState,
                                  FermentationUiCommandPhase::DecisionOnly),
                    409U);
    assertProjected(
        commandResult(Category::Busy, RunPersistenceResultStatus::Busy,
                      FermentationUiCommandPhase::OwningOutcome),
        409U);
    assertProjected(commandResult(Category::Rejected,
                                  RunPersistenceResultStatus::StaleDecision,
                                  FermentationUiCommandPhase::OwningOutcome),
                    409U);
}

void test_outcome_matrix_rejections_and_capacity_are_typed() {
    for (const auto status :
         {CommandStatus::NotAllowedInState, CommandStatus::InvalidInput,
          CommandStatus::SafetyRejected}) {
        assertProjected(commandResult(Category::Rejected, status,
                                      FermentationUiCommandPhase::DecisionOnly),
                        422U);
    }
    for (const auto status : {RunPersistenceResultStatus::NotEligible,
                              RunPersistenceResultStatus::NotAllowedInState,
                              RunPersistenceResultStatus::InvalidDecision,
                              RunPersistenceResultStatus::TimeMismatch,
                              RunPersistenceResultStatus::TimeWentBackwards,
                              RunPersistenceResultStatus::CounterOverflow,
                              RunPersistenceResultStatus::NotDue,
                              RunPersistenceResultStatus::NoActiveRun}) {
        assertProjected(
            commandResult(Category::Rejected, status,
                          FermentationUiCommandPhase::OwningOutcome),
            422U);
    }
    assertProjected(
        commandResult(Category::Rejected, DecisionStatus::InvalidInput,
                      FermentationUiCommandPhase::DecisionOnly),
        422U);
    assertProjected(
        commandResult(Category::Rejected, CommandStatus::CapacityReached,
                      FermentationUiCommandPhase::DecisionOnly),
        413U);
    assertProjected(commandResult(Category::Rejected,
                                  RunPersistenceResultStatus::CapacityExceeded,
                                  FermentationUiCommandPhase::OwningOutcome),
                    413U);
    assertProjected(commandResult(Category::Rejected,
                                  RunPersistenceResultStatus::WriteFailed,
                                  FermentationUiCommandPhase::OwningOutcome),
                    500U);
}

void test_outcome_matrix_recovery_and_indeterminate_states_fail_closed() {
    for (const auto status :
         {RunPersistenceResultStatus::NotInitialized,
          RunPersistenceResultStatus::RecoveryPending,
          RunPersistenceResultStatus::Blocked,
          RunPersistenceResultStatus::PersistenceIndeterminate,
          RunPersistenceResultStatus::PersistenceCommittedApplyFailed}) {
        assertProjected(
            commandResult(Category::Unavailable, status,
                          FermentationUiCommandPhase::OwningOutcome),
            503U);
    }
    assertProjected(
        commandResult(Category::Rejected, CommandStatus::ContextMissing,
                      FermentationUiCommandPhase::DecisionOnly),
        503U);
    assertProjected(commandResult(Category::Accepted, DecisionStatus::Proposed,
                                  FermentationUiCommandPhase::DecisionOnly),
                    503U);

    // A contradictory category/phase must never be promoted to success.
    assertProjected(
        commandResult(Category::Accepted,
                      RunPersistenceResultStatus::PersistenceIndeterminate,
                      FermentationUiCommandPhase::OwningOutcome),
        503U);
    assertProjected(commandResult(Category::Accepted, CommandStatus::Applied,
                                  FermentationUiCommandPhase::DecisionOnly),
                    503U);
}

void test_application_request_status_mapping_is_fail_closed() {
    for (const auto status :
         {FermentationApplicationRequestStatus::StaleProgramCatalog}) {
        const auto projected = replayOutcome(
            WebRunMutationHandlerTestAccess::projectRequest(status));
        TEST_ASSERT_EQUAL_UINT16(409U, projected.statusCode);
    }
    for (const auto status :
         {FermentationApplicationRequestStatus::ProgramUnavailable,
          FermentationApplicationRequestStatus::InvalidInput}) {
        const auto projected = replayOutcome(
            WebRunMutationHandlerTestAccess::projectRequest(status));
        TEST_ASSERT_EQUAL_UINT16(422U, projected.statusCode);
    }
    for (const auto status :
         {FermentationApplicationRequestStatus::NotInitialized,
          FermentationApplicationRequestStatus::Unavailable,
          FermentationApplicationRequestStatus::Overflow}) {
        const auto projected = replayOutcome(
            WebRunMutationHandlerTestAccess::projectRequest(status));
        TEST_ASSERT_EQUAL_UINT16(503U, projected.statusCode);
    }
}

CrossRolePlausibilityContext validOwningEvidence() {
    CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

struct Fixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    FermentationApplication application;
    DeterministicRandom random;
    DeterministicReplayDigest replayDigest;
    WebSessionManager sessions;
    WebRunMutationHandler handler;
    WebSessionResult session;

    Fixture()
        : sessions(random, replayDigest),
          handler(application, sessions, timeSource) {
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
        application.publishOwningRuntimeEvidence(validOwningEvidence());
        session = sessions.create(timeSource.monotonicMillis());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(WebSessionStatus::Created),
                              static_cast<int>(session.status));
        TEST_ASSERT_TRUE(session.handle.has_value());
    }
};

std::string expectedJson(const FermentationUiExpectedRevisions& expected) {
    std::ostringstream json;
    json << "{\"s\":" << expected.expectedStateSequence;
    if (expected.expectedRunRevision.has_value())
        json << ",\"r\":" << *expected.expectedRunRevision;
    if (expected.expectedMessageRevision.has_value())
        json << ",\"m\":" << *expected.expectedMessageRevision;
    if (expected.expectedFaultRevision.has_value())
        json << ",\"f\":" << *expected.expectedFaultRevision;
    if (expected.expectedRecoveryEpisodeRevision.has_value())
        json << ",\"e\":" << *expected.expectedRecoveryEpisodeRevision;
    if (expected.expectedUserConfigurationRevision.has_value()) {
        json << ",\"u\":\""
             << expected.expectedUserConfigurationRevision->value() << '"';
    }
    if (expected.expectedProgramCatalogRevision.has_value())
        json << ",\"c\":\"" << expected.expectedProgramCatalogRevision->value()
             << '"';
    json << '}';
    return json.str();
}

std::string mutationBody(const WebRunMutationDto& dto) {
    std::ostringstream json;
    json << std::setprecision(17)
         << "{\"v\":1,\"r\":" << expectedJson(dto.expected) << ",\"i\":{";
    std::visit(
        [&json](const auto& intent) {
            using Intent = std::decay_t<decltype(intent)>;
            if constexpr (std::is_same_v<
                              Intent, FermentationUiStartManualTimedIntent>) {
                const auto& values = intent.values;
                json << "\"t\":\"start-manual-timed\",\"x\":"
                     << values.targetTemperatureCelsius
                     << ",\"d\":" << values.durationMinutes
                     << ",\"s\":\"air\",\"h\":"
                     << (values.preheatEnabled ? "true" : "false")
                     << ",\"q\":" << values.qualificationBandCelsius
                     << ",\"qd\":" << values.qualificationDurationMinutes
                     << ",\"tr\":" << values.maximumTargetReachMinutes
                     << ",\"c\":\"finish-without-cooling\"";
            } else if constexpr (std::is_same_v<
                                     Intent,
                                     FermentationUiAcknowledgeMessageIntent>) {
                json << "\"t\":\"ack-message\",\"id\":" << intent.messageId;
            } else if constexpr (std::is_same_v<
                                     Intent, FermentationUiMuteMessageIntent>) {
                json << "\"t\":\"mute-message\",\"id\":" << intent.messageId;
            } else if constexpr (std::is_same_v<
                                     Intent,
                                     FermentationUiStartProgramIntent>) {
                json << "\"t\":\"start-program\",\"c\":{"
                     << "\"p\":\"" << intent.candidate.programId << "\"";
                if (intent.candidate.targetTemperatureCelsius.has_value())
                    json << ",\"x\":"
                         << *intent.candidate.targetTemperatureCelsius;
                if (intent.candidate.fermentationDurationMinutes.has_value())
                    json << ",\"d\":"
                         << *intent.candidate.fermentationDurationMinutes;
                if (intent.candidate.preheatEnabled.has_value())
                    json << ",\"h\":"
                         << (*intent.candidate.preheatEnabled ? "true"
                                                              : "false");
                if (intent.candidate.sensorMode.has_value())
                    json << ",\"s\":\""
                         << (*intent.candidate.sensorMode == RunSensorMode::Air
                                 ? "air"
                                 : "product")
                         << "\"";
                if (intent.candidate.completionMode.has_value()) {
                    const auto mode = *intent.candidate.completionMode;
                    const char* name = "finish-without-cooling";
                    if (mode == CompletionMode::CoolThenFinish)
                        name = "cool-then-finish";
                    else if (mode == CompletionMode::CoolAndHoldForDuration)
                        name = "cool-and-hold-for-duration";
                    else if (mode == CompletionMode::CoolAndHoldUntilManualStop)
                        name = "cool-and-hold-until-manual-stop";
                    json << ",\"c\":\"" << name << "\"";
                }
                if (intent.candidate.coolingTargetCelsius.has_value())
                    json << ",\"k\":" << *intent.candidate.coolingTargetCelsius;
                if (intent.candidate.holdDurationMinutes.has_value())
                    json << ",\"l\":" << *intent.candidate.holdDurationMinutes;
                json << "}";
            }
        },
        dto.intent);
    json << "}}";
    return json.str();
}

device_platform::HttpRequest makeRequest(const Fixture& fixture,
                                         const WebRunMutationDto& dto) {
    device_platform::HttpRequest request;
    request.method = "POST";
    request.path = "/internal/ui/run";
    request.body = mutationBody(dto);
    request.metadata.host = "fermenter.local";
    request.metadata.contentType = "application/json; charset=utf-8";
    request.metadata.cookie = "FSSESSION=" + fixture.session.cookieValue;
    request.metadata.csrfToken = fixture.session.csrfToken;
    request.metadata.mutationSeq = "1";
    request.metadata.origin = "http://fermenter.local";
    request.metadata.secFetchSite = "same-origin";
    return request;
}

// A RAM-owned acknowledge of the message installed by installMessage(): the
// regular accepted mutation used by the generic handler tests (a manual run
// needs the technical run limits of an owner that does not exist yet, O5).
constexpr std::uint32_t kHandlerMessageId = 7U;

WebRunMutationDto startRequest(const FermentationApplication& application) {
    return {application.uiSnapshot().revisions,
            FermentationUiAcknowledgeMessageIntent{kHandlerMessageId}};
}

device_platform::StateStoreKey runHeadKey() {
    const auto key = device_platform::StateStoreKey::create("rh0");
    TEST_ASSERT_TRUE(key.key.has_value());
    return *key.key;
}

void installMessage(FermentationApplication& application, std::uint32_t id) {
    auto& state = FermentationApplicationTestAccess::runtimeState(application);
    state.messageCount = 1U;
    state.messageRevision = 0U;
    state.messages[0] = RuntimeMessage{};
    state.messages[0].id = id;
}

void assertRamOwnedMessageMutation(bool mute) {
    Fixture fixture;
    constexpr std::uint32_t kMessageId = 7U;
    installMessage(fixture.application, kMessageId);
    const auto before = fixture.store.read(runHeadKey(), 8240U);

    WebRunMutationDto dto{
        fixture.application.uiSnapshot().revisions,
        mute ? FermentationUiEnvelopePayload{FermentationUiMuteMessageIntent{
                   kMessageId}}
             : FermentationUiEnvelopePayload{
                   FermentationUiAcknowledgeMessageIntent{kMessageId}}};
    auto request = makeRequest(fixture, dto);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_EQUAL_STRING("{\"outcome\":\"applied\"}",
                             completeBody(response).c_str());

    const auto& message =
        FermentationApplicationTestAccess::runtimeState(fixture.application)
            .messages[0];
    TEST_ASSERT_EQUAL_INT(mute ? 0 : 1, message.acknowledged ? 1 : 0);
    TEST_ASSERT_EQUAL_INT(mute ? 1 : 0, message.acousticMuted ? 1 : 0);
    const auto after = fixture.store.read(runHeadKey(), 8240U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(before.status),
                          static_cast<int>(after.status));
    TEST_ASSERT_EQUAL_STRING(before.value.c_str(), after.value.c_str());
}

void test_handler_applies_ram_owned_ack_without_run_persistence() {
    assertRamOwnedMessageMutation(false);
}

void test_handler_applies_ram_owned_mute_without_run_persistence() {
    assertRamOwnedMessageMutation(true);
}

void test_handler_requires_session_csrf_same_origin_and_json() {
    Fixture fixture;
    auto dto = startRequest(fixture.application);
    device_platform::HttpResponse response;
    auto request = makeRequest(fixture, dto);

    const std::string invalidCsrfValue = "wrong-token";
    request.metadata.csrfToken = invalidCsrfValue;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(403U, response.statusCode);

    request = makeRequest(fixture, dto);
    request.metadata.origin = "http://other.local";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(403U, response.statusCode);

    request = makeRequest(fixture, dto);
    request.metadata.contentType = "text/plain";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(415U, response.statusCode);

    request = makeRequest(fixture, dto);
    request.metadata.cookie.reset();
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);

    request = makeRequest(fixture, dto);
    request.metadata.mutationSeq = "0";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);

    request = makeRequest(fixture, dto);
    request.body = "{";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);

    request = makeRequest(fixture, dto);
    request.body.assign(600U, 'x');
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(413U, response.statusCode);

    request = makeRequest(fixture, dto);
    request.method = "GET";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(405U, response.statusCode);
    TEST_ASSERT_FALSE(fixture.handler.handle(
        device_platform::HttpRequest{"POST", "/unrelated", "{}", {}},
        response));

    const auto sequence = fixture.sessions.mutationSequence(
        *fixture.session.handle, fixture.timeSource.monotonicMillis());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MutationSequenceState::Available),
                          static_cast<int>(sequence.state));
    TEST_ASSERT_EQUAL_UINT64(1U, sequence.nextMutationSeq);
}

void test_handler_runs_prepare_confirm_apply_and_replays_exact_result() {
    Fixture fixture;
    installMessage(fixture.application, kHandlerMessageId);
    const auto dto = startRequest(fixture.application);
    const auto request = makeRequest(fixture, dto);
    device_platform::HttpResponse first;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, first));
    TEST_ASSERT_EQUAL_UINT16(200U, first.statusCode);
    TEST_ASSERT_EQUAL_STRING("{\"outcome\":\"applied\"}",
                             completeBody(first).c_str());
    const auto afterFirst = fixture.application.uiSnapshot();
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::runtimeState(fixture.application)
            .messages[0]
            .acknowledged);

    device_platform::HttpResponse replay;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, replay));
    TEST_ASSERT_EQUAL_UINT16(first.statusCode, replay.statusCode);
    TEST_ASSERT_EQUAL_STRING(first.contentType.c_str(),
                             replay.contentType.c_str());
    TEST_ASSERT_EQUAL_STRING(completeBody(first).c_str(),
                             completeBody(replay).c_str());
    const auto afterReplay = fixture.application.uiSnapshot();
    TEST_ASSERT_EQUAL_UINT32(afterFirst.revisions.expectedStateSequence,
                             afterReplay.revisions.expectedStateSequence);

    auto altered = request;
    altered.body =
        "{\"v\":1,\"r\":{\"s\":0},"
        "\"i\":{\"t\":\"reset-fault\"}}";
    device_platform::HttpResponse conflict;
    TEST_ASSERT_TRUE(fixture.handler.handle(altered, conflict));
    TEST_ASSERT_EQUAL_UINT16(409U, conflict.statusCode);
    TEST_ASSERT_EQUAL_UINT32(
        afterFirst.revisions.expectedStateSequence,
        fixture.application.uiSnapshot().revisions.expectedStateSequence);
}

void test_handler_maps_stale_revision_and_invalid_program_without_mutation() {
    Fixture fixture;
    installMessage(fixture.application, kHandlerMessageId);
    auto staleDto = startRequest(fixture.application);
    ++staleDto.expected.expectedStateSequence;
    auto request = makeRequest(fixture, staleDto);
    device_platform::HttpResponse stale;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, stale));
    TEST_ASSERT_EQUAL_UINT16(409U, stale.statusCode);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::Standby),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));

    WebRunMutationDto unavailableProgram{
        fixture.application.uiSnapshot().revisions,
        FermentationUiStartProgramIntent{FermentationUiStartCandidate{
            "not-installed", std::nullopt, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt}}};
    request = makeRequest(fixture, unavailableProgram);
    request.metadata.mutationSeq = "2";
    device_platform::HttpResponse rejected;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, rejected));
    TEST_ASSERT_EQUAL_UINT16(422U, rejected.statusCode);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::Standby),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

// O5 / #172 S9: no web request can start a manual run or a cooling plan while
// no owner of the technical run limits exists, even with complete, valid
// caller-supplied limits; the Application answers Unavailable (503) and
// nothing changes.
void test_handler_refuses_manual_runs_and_cooling_plans_without_a_limits_owner() {
    Fixture fixture;
    const std::string plan =
        "{\"x\":8.0,\"s\":\"air\",\"h\":false,\"q\":0.5,\"qd\":10,"
        "\"tr\":60}";
    const std::array<std::string, 4U> intents{
        "{\"t\":\"start-manual-timed\",\"x\":30.5,\"d\":120,"
        "\"s\":\"air\",\"h\":false,\"q\":0.5,\"qd\":10,\"tr\":180,"
        "\"c\":\"finish-without-cooling\"}",
        "{\"t\":\"start-manual-holding\",\"p\":" + plan + "}",
        "{\"t\":\"stop-run\",\"o\":\"abort-and-cool\",\"p\":" + plan + "}",
        "{\"t\":\"complete-run\",\"c\":true,\"p\":" + plan + "}"};
    const auto before = fixture.application.uiSnapshot();
    std::uint32_t sequence = 1U;
    for (const auto& intent : intents) {
        const auto dto = startRequest(fixture.application);
        auto request = makeRequest(fixture, dto);
        request.body = "{\"v\":1,\"r\":" + expectedJson(dto.expected) +
                       ",\"i\":" + intent + "}";
        request.metadata.mutationSeq = std::to_string(sequence++);
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.handler.handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(FermentationHomeMode::Standby),
            static_cast<int>(fixture.application.uiSnapshot().home.mode));
    }
    TEST_ASSERT_EQUAL_UINT32(
        before.revisions.expectedStateSequence,
        fixture.application.uiSnapshot().revisions.expectedStateSequence);
    const auto head = fixture.store.read(runHeadKey(), 8240U);
    TEST_ASSERT_TRUE(head.status !=
                     device_platform::StateStoreReadStatus::Success);
}

std::string validManualMutationJson() {
    return "{\"v\":1,\"r\":{\"s\":7},"
           "\"i\":{\"t\":\"start-manual-timed\","
           "\"x\":30.5,\"d\":120,\"s\":\"air\",\"h\":false,"
           "\"q\":0.5,\"qd\":10,\"tr\":180,"
           "\"c\":\"finish-without-cooling\"}}";
}

std::string manualTimedMutationJson(
    double targetTemperature, std::uint32_t duration, bool preheat,
    std::optional<std::uint32_t> productWait, double qualificationBand,
    std::uint32_t qualificationDuration, std::uint32_t targetReach,
    const char* completionMode, std::optional<double> coolingTarget = {},
    std::optional<std::uint32_t> holdDuration = {}) {
    std::ostringstream body;
    body << "{\"v\":1,\"r\":{\"s\":7},"
            "\"i\":{\"t\":\"start-manual-timed\",\"x\":"
         << targetTemperature << ",\"d\":" << duration
         << ",\"s\":\"air\",\"h\":" << (preheat ? "true" : "false");
    if (productWait.has_value()) body << ",\"w\":" << *productWait;
    body << ",\"q\":" << qualificationBand
         << ",\"qd\":" << qualificationDuration << ",\"tr\":" << targetReach
         << ",\"c\":\"" << completionMode << "\"";
    if (coolingTarget.has_value()) body << ",\"k\":" << *coolingTarget;
    if (holdDuration.has_value()) body << ",\"l\":" << *holdDuration;
    body << "}}";
    return body.str();
}

std::string manualPlanJson(double targetTemperature = 30.0,
                           bool preheat = false,
                           std::optional<std::uint32_t> productWait = {},
                           double qualificationBand = 0.5,
                           std::uint32_t qualificationDuration = 10U,
                           std::uint32_t targetReach = 180U) {
    std::ostringstream body;
    body << "{\"x\":" << targetTemperature
         << ",\"s\":\"air\",\"h\":" << (preheat ? "true" : "false");
    if (productWait.has_value()) body << ",\"w\":" << *productWait;
    body << ",\"q\":" << qualificationBand
         << ",\"qd\":" << qualificationDuration << ",\"tr\":" << targetReach
         << "}";
    return body.str();
}

std::string startProgramMutationJson(
    const char* completionMode, std::optional<double> targetTemperature = {},
    std::optional<std::uint32_t> duration = {},
    std::optional<double> coolingTarget = {},
    std::optional<std::uint32_t> holdDuration = {}) {
    std::ostringstream body;
    body << "{\"v\":1,\"r\":{\"s\":7},"
            "\"i\":{\"t\":\"start-program\",\"c\":{\"p\":\"program\"";
    if (targetTemperature.has_value()) body << ",\"x\":" << *targetTemperature;
    if (duration.has_value()) body << ",\"d\":" << *duration;
    if (completionMode != nullptr) {
        body << ",\"c\":\"" << completionMode << "\"";
    }
    if (coolingTarget.has_value()) body << ",\"k\":" << *coolingTarget;
    if (holdDuration.has_value()) body << ",\"l\":" << *holdDuration;
    body << "}}}";
    return body.str();
}

void assertMutationDecodeStatus(const std::string& body, bool expectedSuccess) {
    WebRunMutationDto decoded;
    decoded.intent = FermentationUiAcknowledgeMessageIntent{99U};
    const auto status = decodeWebRunMutation(body, decoded);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(expectedSuccess ? WebRunMutationDecodeStatus::Success
                                         : WebRunMutationDecodeStatus::Invalid),
        static_cast<int>(status));
    if (!expectedSuccess) {
        TEST_ASSERT_EQUAL_UINT32(
            99U,
            std::get<FermentationUiAcknowledgeMessageIntent>(decoded.intent)
                .messageId);
    }
}

void test_mutation_codec_rejects_invalid_bodies_without_partial_dto() {
    WebRunMutationDto decoded;
    const auto valid = validManualMutationJson();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebRunMutationDecodeStatus::Success),
        static_cast<int>(decodeWebRunMutation(valid, decoded)));
    TEST_ASSERT_EQUAL_UINT32(7U, decoded.expected.expectedStateSequence);
    TEST_ASSERT_FALSE(
        std::get<FermentationUiStartManualTimedIntent>(decoded.intent)
            .values.preheatEnabled);

    WebRunMutationDto sentinel;
    sentinel.intent = FermentationUiAcknowledgeMessageIntent{99U};
    const auto rejectsWithoutMutation = [&sentinel](const std::string& body) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WebRunMutationDecodeStatus::Invalid),
            static_cast<int>(decodeWebRunMutation(body, sentinel)));
        TEST_ASSERT_EQUAL_UINT32(
            99U,
            std::get<FermentationUiAcknowledgeMessageIntent>(sentinel.intent)
                .messageId);
    };

    auto rawNul = valid;
    rawNul.push_back('\0');
    rejectsWithoutMutation(rawNul);
    rejectsWithoutMutation("{\"v\":1,\"r\":{},\"i\":{\"t\":\"reset-fault\"}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":7},\"extra\":true,"
        "\"i\":{\"t\":\"reset-fault\"}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":7},"
        "\"i\":{\"t\":\"start-manual-timed\","
        "\"x\":30,\"d\":1,\"s\":\"air\",\"h\":\"false\","
        "\"q\":0.5,\"qd\":10,\"tr\":180,"
        "\"c\":\"finish-without-cooling\"}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":7},"
        "\"i\":{\"t\":\"start-manual-timed\","
        "\"x\":1e999,\"d\":1,\"s\":\"air\",\"h\":false,"
        "\"q\":0.5,\"qd\":10,\"tr\":180,"
        "\"c\":\"finish-without-cooling\"}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":7},"
        "\"i\":{\"t\":\"start-manual-timed\","
        "\"x\":30,\"d\":4294967296,\"s\":\"air\",\"h\":false,"
        "\"q\":0.5,\"qd\":10,\"tr\":180,"
        "\"c\":\"finish-without-cooling\"}}");
    rejectsWithoutMutation(valid.substr(0U, valid.size() - 1U));
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":7},"
        "\"i\":{\"t\":\"reset-fault\",\"extra\":[]}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":7},\"i\":{\"t\":\"reset-fault\"},"
        "\"deep\":[[[[[0]]]]]}");
    rejectsWithoutMutation(
        "{\"v\":1,\"v\\u0000ignored\":2,\"r\":{\"s\":7},"
        "\"i\":{\"t\":\"reset-fault\"}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":0},\"i\":{"
        "\"t\":\"start-program\",\"c\":{\"p\":\"ab\\u0000cd\"}}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":0},\"i\":{"
        "\"t\":\"start-program\",\"c\":{\"p\":\"Upper-case\"}}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":0},\"i\":{"
        "\"t\":\"start-program\",\"c\":{\"p\":\"bad--id\"}}}");
    rejectsWithoutMutation(
        "{\"v\":1,\"r\":{\"s\":0},\"i\":{"
        "\"t\":\"start-program\",\"c\":{\"p\":\"-leading\"}}}");
    const std::string overlongProgramId =
        "{\"v\":1,\"r\":{\"s\":0},\"i\":{"
        "\"t\":\"start-program\",\"c\":{\"p\":\"" +
        std::string(configuration_limits::kMaximumProgramIdBytes + 1U, 'p') +
        "\"}}}";
    rejectsWithoutMutation(overlongProgramId);

    const std::string oversized(kMaximumWebRunMutationBodyBytes + 1U, ' ');
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebRunMutationDecodeStatus::TooLarge),
        static_cast<int>(decodeWebRunMutation(oversized, sentinel)));
    auto exactLimit = valid;
    exactLimit.append(kMaximumWebRunMutationBodyBytes - exactLimit.size(), ' ');
    TEST_ASSERT_EQUAL_UINT32(kMaximumWebRunMutationBodyBytes,
                             exactLimit.size());
    WebRunMutationDto exactLimitDecoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebRunMutationDecodeStatus::Success),
        static_cast<int>(decodeWebRunMutation(exactLimit, exactLimitDecoded)));
    for (std::size_t index = 0U; index < valid.size(); ++index) {
        auto changed = valid;
        changed[index] = (index % 2U) == 0U ? '\\' : '\0';
        (void)decodeWebRunMutation(changed, sentinel);
    }
    TEST_ASSERT_EQUAL_UINT32(
        99U, std::get<FermentationUiAcknowledgeMessageIntent>(sentinel.intent)
                 .messageId);
}

void test_mutation_codec_decodes_every_closed_application_intent() {
    const std::array<std::pair<const char*, std::size_t>, 8U> cases{{
        {"{\"v\":1,\"r\":{\"s\":0},\"i\":{\"t\":\"start-manual-holding\","
         "\"p\":{\"x\":30,\"s\":\"product\",\"h\":false,"
         "\"q\":0.5,\"qd\":10,\"tr\":180}}}",
         1U},
        {"{\"v\":1,\"r\":{\"s\":0},\"i\":{\"t\":\"stop-run\","
         "\"o\":\"abort-and-turn-off\"}}",
         3U},
        {"{\"v\":1,\"r\":{\"s\":0},\"i\":{\"t\":\"complete-run\","
         "\"c\":false}}",
         4U},
        {"{\"v\":1,\"r\":{\"s\":0},\"i\":{\"t\":\"adjust-run\","
         "\"x\":31.5,\"d\":90}}",
         5U},
        {"{\"v\":1,\"r\":{\"s\":0},\"i\":{"
         "\"t\":\"recovery-time-correction\",\"d\":20}}",
         6U},
        {"{\"v\":1,\"r\":{\"s\":0},\"i\":{\"t\":\"reset-fault\"}}", 9U},
        {"{\"v\":1,\"r\":{\"s\":0},\"i\":{\"t\":\"sensor-selection\","
         "\"a\":\"recheck-product\"}}",
         10U},
        {"{\"v\":1,\"r\":{\"s\":0},\"i\":{\"t\":\"stop-run\","
         "\"o\":\"abort-and-cool\",\"p\":{\"x\":25,\"s\":\"air\","
         "\"h\":false,\"q\":0.5,\"qd\":10,\"tr\":180}}}",
         3U},
    }};
    for (const auto& item : cases) {
        WebRunMutationDto decoded;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WebRunMutationDecodeStatus::Success),
            static_cast<int>(decodeWebRunMutation(item.first, decoded)));
        TEST_ASSERT_EQUAL_UINT32(item.second, decoded.intent.index());
    }
}

void test_mutation_codec_validates_static_field_ranges_and_completion() {
    const auto manual = [](double target, std::uint32_t duration,
                           double qualificationBand) {
        return manualTimedMutationJson(target, duration, false, std::nullopt,
                                       qualificationBand, 10U, 180U,
                                       "finish-without-cooling");
    };

    assertMutationDecodeStatus(
        manual(program_limits::kMinimumFermentationTemperatureCelsius, 1U,
               program_limits::kMinimumQualificationBandCelsius),
        true);
    assertMutationDecodeStatus(
        manual(program_limits::kMaximumFermentationTemperatureCelsius,
               program_limits::kMaximumFermentationDurationMinutes,
               program_limits::kMaximumQualificationBandCelsius),
        true);
    assertMutationDecodeStatus(manual(3.99, 1U, 0.5), false);
    assertMutationDecodeStatus(manual(45.01, 1U, 0.5), false);
    assertMutationDecodeStatus(manual(30.0, 0U, 0.5), false);
    assertMutationDecodeStatus(manual(30.0, 20161U, 0.5), false);
    assertMutationDecodeStatus(manual(30.0, 1U, 0.09), false);
    assertMutationDecodeStatus(manual(30.0, 1U, 2.01), false);

    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, true,
                                program_limits::kMinimumProductWaitMinutes, 0.5,
                                10U, 180U, "finish-without-cooling"),
        true);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, true,
                                program_limits::kMaximumProductWaitMinutes, 0.5,
                                10U, 180U, "finish-without-cooling"),
        true);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, 1U, 0.5, 10U, 180U,
                                "finish-without-cooling"),
        false);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, true, std::nullopt, 0.5, 10U, 180U,
                                "finish-without-cooling"),
        false);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, true, 0U, 0.5, 10U, 180U,
                                "finish-without-cooling"),
        false);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, true, 1441U, 0.5, 10U, 180U,
                                "finish-without-cooling"),
        false);

    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, std::nullopt, 0.5, 10U, 180U,
                                "finish-without-cooling"),
        true);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, std::nullopt, 0.5, 10U, 180U,
                                "cool-then-finish", 4.0),
        true);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, std::nullopt, 0.5, 10U, 180U,
                                "cool-then-finish"),
        false);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, std::nullopt, 0.5, 10U, 180U,
                                "cool-then-finish", 4.0, 1U),
        false);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, std::nullopt, 0.5, 10U, 180U,
                                "cool-and-hold-for-duration", 25.0,
                                program_limits::kMinimumHoldDurationMinutes),
        true);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, std::nullopt, 0.5, 10U, 180U,
                                "cool-and-hold-for-duration", 25.0),
        false);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, std::nullopt, 0.5, 10U, 180U,
                                "cool-and-hold-for-duration", 25.0, 0U),
        false);
    assertMutationDecodeStatus(
        manualTimedMutationJson(30.0, 1U, false, std::nullopt, 0.5, 10U, 180U,
                                "cool-and-hold-until-manual-stop", 25.0),
        true);
}

void test_mutation_codec_rejects_invalid_manual_plans_and_variant_shapes() {
    const auto holding = [](const std::string& plan) {
        return std::string(
                   "{\"v\":1,\"r\":{\"s\":7},\"i\":{"
                   "\"t\":\"start-manual-holding\",\"p\":") +
               plan + "}}";
    };
    assertMutationDecodeStatus(holding(manualPlanJson()), true);
    assertMutationDecodeStatus(holding(manualPlanJson(3.9)), false);
    assertMutationDecodeStatus(holding(manualPlanJson(30.0, true)), false);
    assertMutationDecodeStatus(
        holding(manualPlanJson(30.0, true, 1U, 0.5, 10U, 180U)), true);
    assertMutationDecodeStatus(holding(manualPlanJson(30.0, false, 1U)), false);
    assertMutationDecodeStatus(
        holding(manualPlanJson(30.0, false, std::nullopt, 0.5, 0U, 180U)),
        false);

    const auto stop = [](const char* option,
                         const std::optional<std::string>& plan = {}) {
        std::string body = std::string(
                               "{\"v\":1,\"r\":{\"s\":7},"
                               "\"i\":{\"t\":\"stop-run\","
                               "\"o\":\"") +
                           option + "\"";
        if (plan.has_value()) body += ",\"p\":" + *plan;
        return body + "}}";
    };
    assertMutationDecodeStatus(stop("back"), true);
    assertMutationDecodeStatus(stop("back", manualPlanJson()), false);
    assertMutationDecodeStatus(stop("abort-and-turn-off", manualPlanJson()),
                               false);
    assertMutationDecodeStatus(stop("abort-and-cool"), false);
    assertMutationDecodeStatus(stop("abort-and-cool", manualPlanJson()), true);

    const auto complete = [](bool startCooling,
                             const std::optional<std::string>& plan = {}) {
        std::string body = std::string(
                               "{\"v\":1,\"r\":{\"s\":7},"
                               "\"i\":{\"t\":\"complete-run\","
                               "\"c\":") +
                           (startCooling ? "true" : "false");
        if (plan.has_value()) body += ",\"p\":" + *plan;
        return body + "}}";
    };
    assertMutationDecodeStatus(complete(false), true);
    assertMutationDecodeStatus(complete(false, manualPlanJson()), false);
    assertMutationDecodeStatus(complete(true), false);
    assertMutationDecodeStatus(complete(true, manualPlanJson()), true);

    assertMutationDecodeStatus(
        startProgramMutationJson(
            "cool-and-hold-for-duration", 4.0,
            program_limits::kMinimumFermentationDurationMinutes, 4.0, 1U),
        true);
    assertMutationDecodeStatus(
        startProgramMutationJson(
            "cool-and-hold-for-duration", 45.0,
            program_limits::kMaximumFermentationDurationMinutes, 25.0,
            program_limits::kMaximumHoldDurationMinutes),
        true);
    assertMutationDecodeStatus(
        startProgramMutationJson("cool-and-hold-for-duration", 3.9, 1U, 4.0,
                                 1U),
        false);
    assertMutationDecodeStatus(
        startProgramMutationJson("cool-and-hold-for-duration", 30.0, 0U, 4.0,
                                 1U),
        false);
    assertMutationDecodeStatus(
        startProgramMutationJson("finish-without-cooling", 30.0, 1U, 4.0),
        false);
    assertMutationDecodeStatus(
        startProgramMutationJson("cool-then-finish", 30.0, 1U, 4.0, 1U), false);

    assertMutationDecodeStatus(
        "{\"v\":1,\"r\":{\"s\":7},\"i\":{\"t\":\"adjust-run\","
        "\"x\":4,\"d\":0}}",
        true);
    assertMutationDecodeStatus(
        "{\"v\":1,\"r\":{\"s\":7},\"i\":{\"t\":\"adjust-run\","
        "\"x\":45,\"d\":20160}}",
        true);
    assertMutationDecodeStatus(
        "{\"v\":1,\"r\":{\"s\":7},\"i\":{\"t\":\"adjust-run\","
        "\"x\":3.9,\"d\":1}}",
        false);
    assertMutationDecodeStatus(
        "{\"v\":1,\"r\":{\"s\":7},\"i\":{\"t\":\"adjust-run\","
        "\"x\":30,\"d\":20161}}",
        false);
}

void test_mutation_codec_uses_canonical_decimal_revision_strings() {
    const auto bodyForRevision = [](const char* key, const char* value) {
        return std::string("{\"v\":1,\"r\":{\"s\":0,\"") + key + "\":\"" +
               value + "\"},\"i\":{\"t\":\"reset-fault\"}}";
    };
    const auto expectAccepted = [](const std::string& body,
                                   bool userConfiguration,
                                   std::uint64_t expected) {
        WebRunMutationDto decoded;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WebRunMutationDecodeStatus::Success),
            static_cast<int>(decodeWebRunMutation(body, decoded)));
        if (userConfiguration) {
            TEST_ASSERT_TRUE(
                decoded.expected.expectedUserConfigurationRevision.has_value());
            TEST_ASSERT_FALSE(
                decoded.expected.expectedProgramCatalogRevision.has_value());
            TEST_ASSERT_EQUAL_UINT64(
                expected,
                decoded.expected.expectedUserConfigurationRevision->value());
        } else {
            TEST_ASSERT_FALSE(
                decoded.expected.expectedUserConfigurationRevision.has_value());
            TEST_ASSERT_TRUE(
                decoded.expected.expectedProgramCatalogRevision.has_value());
            TEST_ASSERT_EQUAL_UINT64(
                expected,
                decoded.expected.expectedProgramCatalogRevision->value());
        }
    };
    const auto expectRejected = [](const std::string& body) {
        WebRunMutationDto decoded;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WebRunMutationDecodeStatus::Invalid),
            static_cast<int>(decodeWebRunMutation(body, decoded)));
    };

    const std::array<const char*, 2U> revisionKeys{{"u", "c"}};
    for (const auto* key : revisionKeys) {
        const bool userConfiguration = std::strcmp(key, "u") == 0;
        expectRejected(bodyForRevision(key, "0"));
        expectRejected(bodyForRevision(key, "00"));
        expectRejected(bodyForRevision(key, "01"));
        expectAccepted(bodyForRevision(key, "1"), userConfiguration, 1U);
        expectAccepted(bodyForRevision(key, "18446744073709551615"),
                       userConfiguration, UINT64_MAX);
        expectRejected(bodyForRevision(key, "18446744073709551616"));
        expectRejected(std::string("{\"v\":1,\"r\":{\"s\":0,\"") + key +
                       "\":1},\"i\":{\"t\":\"reset-fault\"}}");
    }

    WebRunMutationDto absent;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebRunMutationDecodeStatus::Success),
        static_cast<int>(decodeWebRunMutation("{\"v\":1,\"r\":{\"s\":0},"
                                              "\"i\":{\"t\":\"reset-fault\"}}",
                                              absent)));
    TEST_ASSERT_FALSE(
        absent.expected.expectedUserConfigurationRevision.has_value());
    TEST_ASSERT_FALSE(
        absent.expected.expectedProgramCatalogRevision.has_value());
}

void test_maximum_product_mutation_fits_body_and_exact_replay_budget() {
    FermentationUiExpectedRevisions expected;
    expected.expectedStateSequence = UINT32_MAX;
    expected.expectedRunRevision = UINT32_MAX;
    expected.expectedMessageRevision = UINT32_MAX;
    expected.expectedFaultRevision = UINT32_MAX;
    expected.expectedRecoveryEpisodeRevision = UINT32_MAX;
    expected.expectedUserConfigurationRevision =
        UserConfigurationRevision{UINT64_MAX};
    expected.expectedProgramCatalogRevision =
        ProgramCatalogRevision{UINT64_MAX};
    FermentationUiStartCandidate candidate;
    candidate.programId.assign(configuration_limits::kMaximumProgramIdBytes,
                               'p');
    candidate.targetTemperatureCelsius = 42.75;
    candidate.fermentationDurationMinutes =
        program_limits::kMaximumFermentationDurationMinutes;
    candidate.preheatEnabled = false;
    candidate.sensorMode = RunSensorMode::Product;
    candidate.completionMode = CompletionMode::CoolAndHoldForDuration;
    candidate.coolingTargetCelsius =
        program_limits::kMaximumCoolingTargetCelsius;
    candidate.holdDurationMinutes = program_limits::kMaximumHoldDurationMinutes;
    const WebRunMutationDto source{expected,
                                   FermentationUiStartProgramIntent{candidate}};
    const auto body = mutationBody(source);
    TEST_ASSERT_TRUE_MESSAGE(body.size() <= kMaximumWebRunMutationBodyBytes,
                             std::to_string(body.size()).c_str());

    device_platform::HttpRequest request;
    request.method = "POST";
    request.path = "/internal/ui/run";
    request.body = body;
    DeterministicReplayDigest replayDigest;
    ReplayDigest digest{};
    TEST_ASSERT_TRUE(mutationDigest(request, replayDigest, digest));
    TEST_ASSERT_EQUAL_UINT32(kReplayDigestBytes, digest.size());

    WebRunMutationDto decoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebRunMutationDecodeStatus::Success),
        static_cast<int>(decodeWebRunMutation(body, decoded)));
    TEST_ASSERT_EQUAL_UINT64(
        UINT64_MAX, decoded.expected.expectedProgramCatalogRevision->value());
    const auto& decodedCandidate =
        std::get<FermentationUiStartProgramIntent>(decoded.intent).candidate;
    TEST_ASSERT_EQUAL_UINT32(configuration_limits::kMaximumProgramIdBytes,
                             decodedCandidate.programId.size());
    TEST_ASSERT_FALSE(*decodedCandidate.preheatEnabled);
    TEST_ASSERT_EQUAL_UINT32(
        program_limits::kMaximumFermentationDurationMinutes,
        *decodedCandidate.fermentationDurationMinutes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionMode::CoolAndHoldForDuration),
        static_cast<int>(*decodedCandidate.completionMode));
}

void test_read_only_api_projection_bounds_and_untrusted_values() {
    FermentationUiSnapshot snapshot;
    snapshot.revisions.expectedStateSequence = UINT32_MAX;
    snapshot.revisions.expectedRunRevision = UINT32_MAX;
    snapshot.revisions.expectedMessageRevision = UINT32_MAX;
    snapshot.revisions.expectedFaultRevision = UINT32_MAX;
    snapshot.revisions.expectedRecoveryEpisodeRevision = UINT32_MAX;
    snapshot.revisions.expectedUserConfigurationRevision =
        UserConfigurationRevision{UINT64_MAX};
    snapshot.revisions.expectedProgramCatalogRevision =
        ProgramCatalogRevision{UINT64_MAX};
    snapshot.network.currentMode = device_platform::NetworkMode::UNSELECTED;
    snapshot.network.selectionRequired = true;
    snapshot.temperatures = {
        {FermentationTemperatureRole::CabinetAir, 21.5, {}},
        {FermentationTemperatureRole::Product, 18.25, {}},
        {FermentationTemperatureRole::Cooling, std::nullopt, {}}};
    snapshot.temperatures[0].quality.quality =
        device_platform::SensorQuality::Valid;
    snapshot.temperatures[1].quality.quality =
        device_platform::SensorQuality::Stale;
    snapshot.temperatures[2].quality.quality =
        device_platform::SensorQuality::Failed;
    for (std::uint32_t id = 0U; id < kMaximumWebApiAlertCount; ++id) {
        RuntimeMessage message;
        message.id = UINT32_MAX - id;
        message.code = MessageCode::ProductInsertionRequested;
        message.messageClass = MessageClass::DecisionRequired;
        message.decisionRequired = true;
        snapshot.messages.push_back(MessageView{message});
    }

    std::string status;
    TEST_ASSERT_TRUE(
        encodeWebApiStatus(snapshot, FermentationDeviceInfoView{}, status));
    TEST_ASSERT_TRUE(status.size() <= kMaximumWebApiResponseBodyBytes);
    TEST_ASSERT_NOT_NULL(std::strstr(
        status.c_str(), "\"userConfiguration\":\"18446744073709551615\""));
    TEST_ASSERT_NOT_NULL(std::strstr(
        status.c_str(), "\"programCatalog\":\"18446744073709551615\""));
    TEST_ASSERT_NOT_NULL(
        std::strstr(status.c_str(), "\"networkMode\":\"selection-required\""));
    TEST_ASSERT_NULL(std::strstr(status.c_str(), "UNSELECTED"));
    TEST_ASSERT_NULL(std::strstr(status.c_str(), "password"));

    std::string temperatures;
    TEST_ASSERT_TRUE(encodeWebApiTemperatures(snapshot, temperatures));
    TEST_ASSERT_TRUE(temperatures.size() <= kMaximumWebApiResponseBodyBytes);
    TEST_ASSERT_NOT_NULL(
        std::strstr(temperatures.c_str(), "\"valueCelsius\":21.5"));
    TEST_ASSERT_NOT_NULL(std::strstr(
        temperatures.c_str(),
        "\"quality\":\"stale\",\"valid\":false,\"valueCelsius\":null"));
    TEST_ASSERT_NOT_NULL(std::strstr(
        temperatures.c_str(),
        "\"quality\":\"failed\",\"valid\":false,\"valueCelsius\":null"));
    TEST_ASSERT_NULL(std::strstr(temperatures.c_str(), "appliedOffset"));

    snapshot.temperatures.push_back(
        {FermentationTemperatureRole::CabinetAir, 20.0, {}});
    TEST_ASSERT_FALSE(encodeWebApiTemperatures(snapshot, temperatures));
    snapshot.temperatures.pop_back();

    std::string alerts;
    TEST_ASSERT_TRUE(encodeWebApiAlerts(snapshot, alerts));
    TEST_ASSERT_TRUE(alerts.size() <= kMaximumWebApiResponseBodyBytes);
    TEST_ASSERT_NOT_NULL(std::strstr(alerts.c_str(), "decisionRequired"));
    TEST_ASSERT_NULL(std::strstr(alerts.c_str(), "monotonicMillis"));
    TEST_ASSERT_NULL(std::strstr(alerts.c_str(), "password"));
    snapshot.messages.push_back(MessageView{RuntimeMessage{}});
    TEST_ASSERT_FALSE(encodeWebApiAlerts(snapshot, alerts));
}

void test_read_only_api_routes_are_get_only_and_uncomposed() {
    Fixture fixture;
    WebReadOnlyApiHandler api(fixture.application);
    device_platform::HttpRequest request;
    request.method = "GET";
    request.path = "/api/v1/status";
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_EQUAL_STRING("application/json; charset=utf-8",
                             response.contentType.c_str());
    TEST_ASSERT_TRUE(completeBody(response).size() <=
                     kMaximumWebApiResponseBodyBytes);

    request.path = "/api/v1/temperatures";
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find("temperatures") !=
                     std::string::npos);

    request.path = "/api/v1/alerts";
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find("alerts") !=
                     std::string::npos);

    request.method = "POST";
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(405U, response.statusCode);
    request.method = "GET";
    request.body = "{}";
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);
    request.body.clear();
    request.path = "/api/v1/history";
    TEST_ASSERT_FALSE(api.handle(request, response));
}

void test_composed_dispatcher_preserves_setup_priority_and_single_server() {
    ComposedFixture fixture;
    auto* routes = fixture.http.routes();
    TEST_ASSERT_NOT_NULL(routes);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.http.startCount());

    auto request = makeWebRequest("GET", "/");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(routes->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find("Fermentation") !=
                     std::string::npos);

    request = makeWebRequest("GET", "/api/v1/status");
    TEST_ASSERT_TRUE(routes->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    request = makeWebRequest("POST", "/internal/ui/run");
    TEST_ASSERT_TRUE(routes->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);

    const auto home = fixture.application.applyNetworkMode(
        device_platform::NetworkMode::HOME_WIFI);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(home.status));
    TEST_ASSERT_TRUE(fixture.application.networkSetupFlowActive());
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.http.startCount());

    request = makeWebRequest("GET", "/");
    TEST_ASSERT_TRUE(routes->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find("Network setup") !=
                     std::string::npos);

    request = makeWebRequest("GET", "/api/v1/status");
    TEST_ASSERT_FALSE(routes->handle(request, response));
    char asset[24]{};
    std::snprintf(asset, sizeof(asset), "/assets/app.%08x.js",
                  static_cast<unsigned>(device_platform::computeCrc32IsoHdlc(
                      kWebApplicationAsset)));
    request = makeWebRequest("GET", asset);
    TEST_ASSERT_FALSE(routes->handle(request, response));
    request = makeWebRequest("GET", "/api/network/status");
    TEST_ASSERT_TRUE(routes->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.http.startCount());
}

void test_composed_dispatcher_revokes_sessions_on_network_boundaries() {
    ComposedFixture fixture(true);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto first = loginComposed(fixture);
    const auto second = loginComposed(fixture);

    const auto noChange = fixture.application.applyNetworkMode(
        device_platform::NetworkMode::AP_ONLY);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(noChange.status));
    auto request =
        makeAuthenticatedRequest("GET", "/api/v1/status", first.cookie);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);

    const auto changed = fixture.application.applyNetworkMode(
        device_platform::NetworkMode::HOME_WIFI);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(changed.status));
    assertComposedSessionRejected(fixture, first.cookie);
    assertComposedSessionRejected(fixture, second.cookie);
}

void test_composed_dispatcher_keeps_sessions_on_failed_network_change() {
    ComposedFixture fixture(true);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto session = loginComposed(fixture);
    fixture.network.setStartStatus(
        device_platform::NetworkOperationStatus::Failed);

    const auto failed = fixture.application.applyNetworkMode(
        device_platform::NetworkMode::HOME_WIFI);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::TransportFailure),
        static_cast<int>(failed.status));
    auto* routes = fixture.http.lastRoutes();
    TEST_ASSERT_NOT_NULL(routes);
    auto request =
        makeAuthenticatedRequest("GET", "/api/v1/status", session.cookie);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(routes->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
}

void test_composed_dispatcher_revokes_sessions_for_home_wifi_reconfiguration() {
    ComposedFixture fixture(true);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(
            fixture.application
                .applyNetworkMode(device_platform::NetworkMode::HOME_WIFI)
                .status));
    const auto first = loginComposed(fixture);
    const auto second = loginComposed(fixture);

    const auto reconfigured =
        fixture.application.beginHomeWifiReconfiguration();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(reconfigured.status));
    TEST_ASSERT_TRUE(fixture.application.networkSetupFlowActive());

    auto request =
        makeWebRequest("POST", "/api/network/candidate",
                       "ssid=next-network&password=next-network-password");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find("candidate committed") !=
                     std::string::npos);
    assertComposedSessionRejected(fixture, first.cookie);
    assertComposedSessionRejected(fixture, second.cookie);
}

void test_composed_dispatcher_factory_reset_revokes_old_sessions() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto first = loginComposed(fixture);
    const auto second = loginComposed(fixture);

    const auto reset = fixture.application.beginAuthorizedFactoryReset();
    TEST_ASSERT_TRUE(
        reset.status == ConfigurationRecoveryStatus::FactoryResetCompleted ||
        reset.status ==
            ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable);
    assertComposedSessionFailClosed(fixture, first.cookie);
    assertComposedSessionFailClosed(fixture, second.cookie);

    const auto postResetAuth = fixture.application.webAuthenticationState();
    TEST_ASSERT_TRUE(postResetAuth !=
                     WebAuthenticationState::PasswordProtected);
    if (reset.status == ConfigurationRecoveryStatus::FactoryResetCompleted) {
        TEST_ASSERT_TRUE(postResetAuth ==
                         WebAuthenticationState::Unprovisioned);
    } else {
        TEST_ASSERT_TRUE(
            postResetAuth == WebAuthenticationState::Unprovisioned ||
            postResetAuth == WebAuthenticationState::RecoveryRequired ||
            postResetAuth == WebAuthenticationState::Indeterminate);
    }

    auto request = makeLoginRequest("correct horse battery");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    TEST_ASSERT_TRUE(completeBody(response).find("authenticated") ==
                     std::string::npos);
    if (postResetAuth == WebAuthenticationState::Unprovisioned) {
        TEST_ASSERT_TRUE(completeBody(response).find("provisioning-required") !=
                         std::string::npos);
    } else {
        TEST_ASSERT_TRUE(completeBody(response).find("recovery-required") !=
                         std::string::npos);
    }
}

void test_composed_dispatcher_anonymous_and_recovery_states_fail_closed() {
    ComposedFixture fixture;
    auto request = makeWebRequest("GET", "/");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    // Unprovisioned: the first-time setup form (S6) instead of the old hint.
    TEST_ASSERT_TRUE(completeBody(response).find("/api/v1/provision") !=
                     std::string::npos);
    request = makeWebRequest("GET", "/api/v1/status");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);

    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::setWebPasswordEnabled(
        fixture.application, false));
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordDisabled);
    request = makeWebRequest("GET", "/");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(response.metadata.setCookie.has_value());
    TEST_ASSERT_TRUE(
        shellDocument(response).find("Password protection is disabled") !=
        std::string::npos);
    TEST_ASSERT_TRUE(completeBody(response).find("password-label") !=
                     std::string::npos);
    const auto anonymousCookie = sessionCookieValue(response);
    request =
        makeAuthenticatedRequest("GET", "/api/v1/status", anonymousCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);

    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::setAuthProvisioningState(
            fixture.application, AuthProvisioningState::RecoveryRequired));
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::RecoveryRequired);
    request = makeWebRequest("GET", "/");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    request =
        makeAuthenticatedRequest("GET", "/api/v1/status", anonymousCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
}

void test_composed_dispatcher_authenticates_read_only_sessions_and_expires() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordProtected);

    auto request = makeLoginRequest("wrong");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    TEST_ASSERT_TRUE(completeBody(response).find("invalid-credentials") !=
                     std::string::npos);

    request = makeLoginRequest("correct horse battery");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    const auto firstCookie = sessionCookieValue(response);
    const auto firstCsrf = csrfToken(response);
    TEST_ASSERT_TRUE(response.metadata.setCookie->find("HttpOnly") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(response.metadata.setCookie->find("SameSite=Strict") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(response.metadata.setCookie->find("Path=/") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(response.metadata.setCookie->find("Secure") ==
                     std::string::npos);
    request = makeAuthenticatedRequest("GET", "/", firstCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find("const U=true") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(shellDocument(response).find(
                         "document.documentElement.lang=sw.value=lc") !=
                     std::string::npos);
    // The raw JSON placeholder is gone; the board replaces it.
    TEST_ASSERT_TRUE(completeBody(response).find("id=\"board\"") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(completeBody(response).find("<pre") == std::string::npos);

    request = makeAuthenticatedRequest("GET", "/api/v1/status", firstCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find("password") ==
                     std::string::npos);
    request =
        makeAuthenticatedRequest("GET", "/api/v1/temperatures", firstCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    request = makeAuthenticatedRequest("GET", "/api/v1/alerts", firstCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);

    request = makeAuthenticatedRequest("POST", "/api/v1/logout", firstCookie,
                                       firstCsrf);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(415U, response.statusCode);
    request = makeLogoutRequest(firstCookie, firstCsrf);
    request.metadata.contentType = "text/plain";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(415U, response.statusCode);

    request = makeLoginRequest("correct horse battery");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    const auto secondCookie = sessionCookieValue(response);

    request = makeLogoutRequest(firstCookie, firstCsrf);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(response.metadata.setCookie.has_value());
    request = makeAuthenticatedRequest("GET", "/api/v1/status", firstCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);
    request = makeAuthenticatedRequest("GET", "/api/v1/status", secondCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);

    request = makeLoginRequest("correct horse battery");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    const auto expiringCookie = sessionCookieValue(response);
    fixture.timeSource.advanceMonotonicMillis(kWebSessionIdleLimitMs - 1U);
    request = makeAuthenticatedRequest("GET", "/api/v1/status", expiringCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    fixture.timeSource.advanceMonotonicMillis(2U);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);

    ComposedFixture lockoutFixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        lockoutFixture.application, "correct horse battery", "1234"));
    for (int attempt = 0; attempt < 4; ++attempt) {
        request = makeLoginRequest("wrong password here");
        TEST_ASSERT_TRUE(
            lockoutFixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);
    }
    request = makeLoginRequest("wrong password here");
    TEST_ASSERT_TRUE(lockoutFixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(429U, response.statusCode);
    TEST_ASSERT_TRUE(response.metadata.retryAfter.has_value());
}

void test_application_call_serializer_blocks_cross_thread_and_allows_reentry() {
    Fixture fixture;
    std::mutex synchronization;
    std::condition_variable changed;
    bool workerStarted = false;
    bool workerAcquired = false;

    std::thread worker;
    {
        auto held =
            FermentationApplicationTestAccess::enter(fixture.application);
        static_cast<void>(fixture.application.uiSnapshot());
        worker = std::thread([&] {
            {
                std::lock_guard<std::mutex> lock(synchronization);
                workerStarted = true;
            }
            changed.notify_one();
            auto entered =
                FermentationApplicationTestAccess::enter(fixture.application);
            {
                std::lock_guard<std::mutex> lock(synchronization);
                workerAcquired = true;
            }
            changed.notify_one();
            static_cast<void>(entered);
        });

        {
            std::unique_lock<std::mutex> lock(synchronization);
            TEST_ASSERT_TRUE(changed.wait_for(lock, std::chrono::seconds(1),
                                              [&] { return workerStarted; }));
            TEST_ASSERT_FALSE(workerAcquired);
        }
    }
    {
        std::unique_lock<std::mutex> lock(synchronization);
        TEST_ASSERT_TRUE(changed.wait_for(lock, std::chrono::seconds(1),
                                          [&] { return workerAcquired; }));
    }
    worker.join();
}

}  // namespace

template <typename Predicate>
bool eventually(Predicate predicate) {
    for (int attempt = 0; attempt < 2000; ++attempt) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

std::string authRecordBytes(ComposedFixture& fixture) {
    const auto key = device_platform::StateStoreKey::create(
        configuration_storage_contract::kAuthenticationStoreKey);
    TEST_ASSERT_TRUE(key.key.has_value());
    return fixture.store.read(*key.key, 4096U).value;
}

bool resetFinished(const ConfigurationRecoveryResult& reset) {
    return reset.status == ConfigurationRecoveryStatus::FactoryResetCompleted ||
           reset.status ==
               ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable;
}

void test_factory_reset_drains_running_login_before_touching_the_store() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto oldSession = loginComposed(fixture);
    const auto recordsBefore = authRecordBytes(fixture);
    TEST_ASSERT_FALSE(recordsBefore.empty());

    fixture.kdf.arm();
    WebAuthenticationResult loginResult;
    std::thread login([&] {
        loginResult = fixture.application.authenticateWebPassword(
            "correct horse battery", 1000U);
    });
    TEST_ASSERT_TRUE(fixture.kdf.waitEntered());

    std::atomic<bool> resetDone{false};
    ConfigurationRecoveryResult reset;
    std::thread resetter([&] {
        reset = fixture.application.beginAuthorizedFactoryReset();
        resetDone = true;
    });
    // The reset has closed the gate and waits for the running login.
    TEST_ASSERT_TRUE(eventually([&] {
        return FermentationApplicationTestAccess::authGateClosed(
            fixture.application);
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT_FALSE(resetDone.load());
    TEST_ASSERT_TRUE(authRecordBytes(fixture) == recordsBefore);

    fixture.kdf.release();
    login.join();
    resetter.join();

    // The login finished against the old epoch; the reset then completed.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::Authenticated),
        static_cast<int>(loginResult.status));
    TEST_ASSERT_TRUE(resetFinished(reset));
    assertComposedSessionFailClosed(fixture, oldSession.cookie);
    if (reset.status == ConfigurationRecoveryStatus::FactoryResetCompleted) {
        TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                         WebAuthenticationState::Unprovisioned);
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::authGateClosed(
            fixture.application));
    }
    TEST_ASSERT_TRUE(authRecordBytes(fixture) != recordsBefore);
}

void test_login_after_gate_close_is_fail_closed_without_kdf() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    FermentationApplicationTestAccess::closeAuthGate(fixture.application);
    const auto callsBefore = fixture.kdf.calls();
    const auto closed = fixture.application.authenticateWebPassword(
        "correct horse battery", 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::RecoveryRequired),
        static_cast<int>(closed.status));
    TEST_ASSERT_EQUAL_UINT(callsBefore, fixture.kdf.calls());

    FermentationApplicationTestAccess::reopenAuthGate(fixture.application);
    const auto reopened = fixture.application.authenticateWebPassword(
        "correct horse battery", 2000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::Authenticated),
        static_cast<int>(reopened.status));
    TEST_ASSERT_TRUE(fixture.kdf.calls() > callsBefore);
}

void test_failed_factory_reset_reopens_the_gate_and_keeps_the_domain() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    fixture.store.setNextWriteFault(
        device_platform_test_support::SimulatedPersistentStateStore::
            WriteFault::FailBeforeBegin);
    const auto reset = fixture.application.beginAuthorizedFactoryReset();
    TEST_ASSERT_TRUE(reset.status !=
                     ConfigurationRecoveryStatus::FactoryResetCompleted);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::authGateClosed(fixture.application));
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordProtected);
    const auto login = fixture.application.authenticateWebPassword(
        "correct horse battery", 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::Authenticated),
        static_cast<int>(login.status));
}

void test_reset_may_proceed_right_after_the_domain_returns() {
    // The login maps its result from local values after the token ended; a
    // reset that proceeds immediately must neither deadlock nor disturb it.
    for (int iteration = 0; iteration < 8; ++iteration) {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, "correct horse battery", "1234"));
        fixture.kdf.arm();
        WebAuthenticationResult loginResult;
        std::thread login([&] {
            loginResult = fixture.application.authenticateWebPassword(
                "correct horse battery", 1000U);
        });
        TEST_ASSERT_TRUE(fixture.kdf.waitEntered());
        ConfigurationRecoveryResult reset;
        std::thread resetter(
            [&] { reset = fixture.application.beginAuthorizedFactoryReset(); });
        TEST_ASSERT_TRUE(eventually([&] {
            return FermentationApplicationTestAccess::authGateClosed(
                fixture.application);
        }));
        fixture.kdf.release();
        login.join();
        resetter.join();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WebAuthenticationResultStatus::Authenticated),
            static_cast<int>(loginResult.status));
        TEST_ASSERT_TRUE(resetFinished(reset));
    }
}

// ---- Session issuance vs. trust boundaries ---------------------------------
// The login handler takes the authentication decision (token already ended)
// and then issues the session. These tests drive that exact sequence with a
// trust boundary placed deterministically between the two steps.

void test_stale_protected_login_cannot_create_a_session_after_factory_reset() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto decision = fixture.application.authenticateWebPassword(
        "correct horse battery", 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::Authenticated),
        static_cast<int>(decision.status));

    const auto reset = fixture.application.beginAuthorizedFactoryReset();
    TEST_ASSERT_TRUE(resetFinished(reset));

    const auto issued = fixture.application.issueWebSession(decision, 1001U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebSessionIssueStatus::TrustBoundaryChanged),
        static_cast<int>(issued.status));
    TEST_ASSERT_FALSE(issued.session.handle.has_value());
    TEST_ASSERT_TRUE(issued.session.cookieValue.empty());
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 1002U));
    if (reset.status == ConfigurationRecoveryStatus::FactoryResetCompleted) {
        TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                         WebAuthenticationState::Unprovisioned);
    }
}

void test_protected_login_session_created_before_reset_is_revoked() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto decision = fixture.application.authenticateWebPassword(
        "correct horse battery", 1000U);
    const auto issued = fixture.application.issueWebSession(decision, 1001U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebSessionIssueStatus::Created),
                          static_cast<int>(issued.status));
    TEST_ASSERT_TRUE(issued.session.handle.has_value());
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 1002U));

    const auto reset = fixture.application.beginAuthorizedFactoryReset();
    TEST_ASSERT_TRUE(resetFinished(reset));
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 1003U));
}

void test_stale_disabled_mode_login_cannot_create_a_session_after_reset() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::setWebPasswordEnabled(
        fixture.application, false));
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordDisabled);
    const auto callsBefore = fixture.kdf.calls();
    const auto decision =
        fixture.application.authenticateWebPassword(std::string{}, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::Disabled),
        static_cast<int>(decision.status));
    TEST_ASSERT_EQUAL_UINT(callsBefore, fixture.kdf.calls());

    const auto reset = fixture.application.beginAuthorizedFactoryReset();
    TEST_ASSERT_TRUE(resetFinished(reset));

    const auto issued = fixture.application.issueWebSession(decision, 1001U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebSessionIssueStatus::TrustBoundaryChanged),
        static_cast<int>(issued.status));
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 1002U));
}

void test_stale_login_cannot_create_a_session_after_network_boundaries() {
    ComposedFixture fixture(true);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));

    // Network mode change between decision and issuance.
    auto decision = fixture.application.authenticateWebPassword(
        "correct horse battery", 1000U);
    const auto changed = fixture.application.applyNetworkMode(
        device_platform::NetworkMode::HOME_WIFI);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(changed.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebSessionIssueStatus::TrustBoundaryChanged),
        static_cast<int>(
            fixture.application.issueWebSession(decision, 1001U).status));

    // HOME_WIFI reconfiguration between decision and issuance.
    decision = fixture.application.authenticateWebPassword(
        "correct horse battery", 2000U);
    const auto reconfigured =
        fixture.application.beginHomeWifiReconfiguration();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(reconfigured.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebSessionIssueStatus::TrustBoundaryChanged),
        static_cast<int>(
            fixture.application.issueWebSession(decision, 2001U).status));
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 2002U));
}

void test_concurrent_login_and_factory_reset_never_leave_a_valid_session() {
    for (int iteration = 0; iteration < 8; ++iteration) {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, "correct horse battery", "1234"));
        fixture.kdf.arm();
        device_platform::HttpResponse loginResponse;
        std::thread login([&] {
            const auto request = makeLoginRequest("correct horse battery");
            static_cast<void>(
                fixture.http.routes()->handle(request, loginResponse));
        });
        TEST_ASSERT_TRUE(fixture.kdf.waitEntered());
        ConfigurationRecoveryResult reset;
        std::thread resetter(
            [&] { reset = fixture.application.beginAuthorizedFactoryReset(); });
        TEST_ASSERT_TRUE(eventually([&] {
            return FermentationApplicationTestAccess::authGateClosed(
                fixture.application);
        }));
        fixture.kdf.release();
        login.join();
        resetter.join();
        TEST_ASSERT_TRUE(resetFinished(reset));
        // Whatever the interleaving was: no session survives the reset.
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
            fixture.application, 5000U));
        if (loginResponse.statusCode == 200U) {
            assertComposedSessionFailClosed(fixture,
                                            sessionCookieValue(loginResponse));
        } else {
            TEST_ASSERT_FALSE(loginResponse.metadata.setCookie.has_value());
        }
    }
}

// ---- S3: local release window and web provisioning --------------------------

constexpr const char kPinUnderTest[] = "1234";

std::string passwordUnderTest() { return "correct horse battery"; }

WebProvisionStatus provisionProtected(ComposedFixture& fixture) {
    return fixture.application.provisionWebAccess(
        WebProvisionMode::Protect, passwordUnderTest(), kPinUnderTest);
}

void test_window_opens_only_when_unprovisioned_and_bootstrap_allowed() {
    ComposedFixture fixture;
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    const auto generation =
        FermentationApplicationTestAccess::trustGeneration(fixture.application);
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    // Opening the window is not a trust boundary.
    TEST_ASSERT_EQUAL_UINT64(generation,
                             FermentationApplicationTestAccess::trustGeneration(
                                 fixture.application));
    // Re-opening a valid window is a no-op and never extends it.
    fixture.timeSource.advanceMonotonicMillis(1000U);
    TEST_ASSERT_FALSE(fixture.application.openWebProvisioningWindow());

    // Already provisioned (protected / disabled): cannot open.
    ComposedFixture protectedFixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        protectedFixture.application, passwordUnderTest(), kPinUnderTest));
    TEST_ASSERT_FALSE(protectedFixture.application.openWebProvisioningWindow());
    ComposedFixture disabledFixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        disabledFixture.application, passwordUnderTest(), kPinUnderTest));
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::setWebPasswordEnabled(
        disabledFixture.application, false));
    TEST_ASSERT_FALSE(disabledFixture.application.openWebProvisioningWindow());

    // Recovery required: cannot open.
    ComposedFixture recoveryFixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::forceRootState(
        recoveryFixture.application, AuthProvisioningState::RecoveryRequired));
    TEST_ASSERT_TRUE(recoveryFixture.application.webAuthenticationState() ==
                     WebAuthenticationState::RecoveryRequired);
    TEST_ASSERT_FALSE(recoveryFixture.application.openWebProvisioningWindow());

    // Not started application (no domain, no clock): Indeterminate.
    FermentationApplication notStarted;
    TEST_ASSERT_TRUE(notStarted.webAuthenticationState() ==
                     WebAuthenticationState::Indeterminate);
    TEST_ASSERT_FALSE(notStarted.openWebProvisioningWindow());
    TEST_ASSERT_TRUE(notStarted.provisionWebAccess(WebProvisionMode::Protect,
                                                   passwordUnderTest(),
                                                   kPinUnderTest) ==
                     WebProvisionStatus::RecoveryRequired);
}

void test_window_lasts_exactly_ten_minutes_and_is_not_extended() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    fixture.timeSource.advanceMonotonicMillis(kWebProvisioningWindowMs - 1U);
    TEST_ASSERT_FALSE(fixture.application.openWebProvisioningWindow());
    // Still open one millisecond before expiry: a rejected input is reported
    // as invalid, not as "not allowed".
    TEST_ASSERT_TRUE(fixture.application.provisionWebAccess(
                         WebProvisionMode::Protect, "short", kPinUnderTest) ==
                     WebProvisionStatus::InvalidCredentials);
    fixture.timeSource.advanceMonotonicMillis(1U);
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::NotAllowed);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
}

void test_window_fails_closed_on_backward_clock_observation() {
    ComposedFixture fixture;
    fixture.timeSource.advanceMonotonicMillis(5000U);
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    // Simulates an observation earlier than the recorded opening.
    FermentationApplicationTestAccess::setWindowOpenedAt(fixture.application,
                                                         9000U);
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::NotAllowed);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
}

void test_window_closes_at_boundaries_and_survives_candidate_commit() {
    // Successful network mode change closes the window.
    {
        ComposedFixture fixture(true);
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        const auto changed = fixture.application.applyNetworkMode(
            device_platform::NetworkMode::HOME_WIFI);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(NetworkConfigurationStatus::Applied),
            static_cast<int>(changed.status));
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
    }
    // Failed network mode change leaves the window as it was.
    {
        ComposedFixture fixture(true);
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        fixture.network.setStartStatus(
            device_platform::NetworkOperationStatus::Failed);
        const auto failed = fixture.application.applyNetworkMode(
            device_platform::NetworkMode::HOME_WIFI);
        TEST_ASSERT_TRUE(failed.status != NetworkConfigurationStatus::Applied);
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
    }
    // HOME_WIFI reconfiguration closes; a candidate commit alone does not.
    {
        ComposedFixture fixture(true);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(NetworkConfigurationStatus::Applied),
            static_cast<int>(
                fixture.application
                    .applyNetworkMode(device_platform::NetworkMode::HOME_WIFI)
                    .status));
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        const auto reconfigured =
            fixture.application.beginHomeWifiReconfiguration();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(NetworkConfigurationStatus::Applied),
            static_cast<int>(reconfigured.status));
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));

        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        auto request =
            makeWebRequest("POST", "/api/network/candidate",
                           "ssid=next-network&password=next-network-password");
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
        TEST_ASSERT_FALSE(fixture.application.networkSetupFlowActive());
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
    }
    // Factory reset closes the window.
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        const auto reset = fixture.application.beginAuthorizedFactoryReset();
        TEST_ASSERT_TRUE(resetFinished(reset));
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::NotAllowed);
    }
    // A fresh application (restart) starts with a closed window.
    {
        ComposedFixture fixture;
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::NotAllowed);
    }
}

void test_window_closes_lazily_when_auth_state_is_no_longer_bootstrap_allowed() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::forceRootState(
        fixture.application, AuthProvisioningState::RecoveryRequired));
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::RecoveryRequired);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
}

void test_provision_protect_consumes_window_and_login_works() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    const auto generation =
        FermentationApplicationTestAccess::trustGeneration(fixture.application);
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::Provisioned);
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordProtected);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::trustGeneration(
                         fixture.application) > generation);
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::AlreadyProvisioned);
    // No session was created by the provisioning itself.
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 0U));
    const auto login =
        fixture.application.authenticateWebPassword(passwordUnderTest(), 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::Authenticated),
        static_cast<int>(login.status));
    const auto issued = fixture.application.issueWebSession(login, 1001U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebSessionIssueStatus::Created),
                          static_cast<int>(issued.status));
}

void test_provision_disable_uses_only_service_pin_kdf_and_allows_sessions() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    TEST_ASSERT_TRUE(fixture.application.provisionWebAccess(
                         WebProvisionMode::Disable, "", kPinUnderTest) ==
                     WebProvisionStatus::Provisioned);
    TEST_ASSERT_EQUAL_UINT(1U, fixture.kdf.calls());
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordDisabled);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    const auto decision =
        fixture.application.authenticateWebPassword(std::string{}, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::Disabled),
        static_cast<int>(decision.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebSessionIssueStatus::Created),
        static_cast<int>(
            fixture.application.issueWebSession(decision, 1001U).status));
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::AlreadyProvisioned);
}

void test_provision_without_window_is_not_allowed_without_kdf_or_write() {
    ComposedFixture fixture;
    const auto recordsBefore = authRecordBytes(fixture);
    const auto callsBefore = fixture.kdf.calls();
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::NotAllowed);
    TEST_ASSERT_EQUAL_UINT(callsBefore, fixture.kdf.calls());
    TEST_ASSERT_TRUE(authRecordBytes(fixture) == recordsBefore);
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::Unprovisioned);
}

void test_provision_state_matrix_never_calls_bootstrap() {
    // Protected / Disabled -> AlreadyProvisioned.
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        const auto calls = fixture.kdf.calls();
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::AlreadyProvisioned);
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::setWebPasswordEnabled(
                fixture.application, false));
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::AlreadyProvisioned);
        TEST_ASSERT_EQUAL_UINT(calls, fixture.kdf.calls());
    }
    // RecoveryRequired -> RecoveryRequired.
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::forceRootState(
            fixture.application, AuthProvisioningState::RecoveryRequired));
        const auto calls = fixture.kdf.calls();
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::RecoveryRequired);
        TEST_ASSERT_EQUAL_UINT(calls, fixture.kdf.calls());
    }
}

void test_provision_input_and_failure_contracts() {
    // Domain-invalid inputs keep the device repeatable.
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        const auto recordsBefore = authRecordBytes(fixture);
        TEST_ASSERT_TRUE(
            fixture.application.provisionWebAccess(WebProvisionMode::Protect,
                                                   "short", kPinUnderTest) ==
            WebProvisionStatus::InvalidCredentials);
        TEST_ASSERT_TRUE(fixture.application.provisionWebAccess(
                             WebProvisionMode::Protect, passwordUnderTest(),
                             "12") == WebProvisionStatus::InvalidCredentials);
        TEST_ASSERT_TRUE(fixture.application.provisionWebAccess(
                             WebProvisionMode::Disable, passwordUnderTest(),
                             kPinUnderTest) ==
                         WebProvisionStatus::InvalidCredentials);
        TEST_ASSERT_TRUE(authRecordBytes(fixture) == recordsBefore);
        TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                         WebAuthenticationState::Unprovisioned);
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::Provisioned);
    }
    // Persistence failure before the first write: still Unprovisioned.
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        fixture.store.setNextWriteFault(
            device_platform_test_support::SimulatedPersistentStateStore::
                WriteFault::FailBeforeBegin);
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::Failed);
        TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                         WebAuthenticationState::Unprovisioned);
        // Retryable pre-commit failure: the release stays valid.
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::Provisioned);
    }
    // KDF failure after the first root write: existing domain recovery
    // contract, no session.
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        fixture.kdf.setFailing(true);
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::Failed);
        fixture.kdf.setFailing(false);
        TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                         WebAuthenticationState::RecoveryRequired);
        // The window ends immediately, without a second call.
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
            fixture.application, 0U));
        TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                         WebProvisionStatus::RecoveryRequired);
    }
    // Random failure in disabled mode: fail closed.
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        fixture.random.fail = true;
        TEST_ASSERT_TRUE(fixture.application.provisionWebAccess(
                             WebProvisionMode::Disable, "", kPinUnderTest) ==
                         WebProvisionStatus::Failed);
        fixture.random.fail = false;
        TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                         WebAuthenticationState::RecoveryRequired);
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
    }
}

// ---- S4: POST /api/v1/provision ---------------------------------------------

std::string provisionBodyProtect(const std::string& webSecret,
                                 const std::string& pin) {
    return "{\"mode\":\"protect\",\"password\":\"" + webSecret +
           "\",\"servicePin\":\"" + pin + "\"}";
}

std::string provisionBodyDisable(const std::string& pin,
                                 const std::string& confirmValue = "true") {
    return "{\"mode\":\"disable\",\"servicePin\":\"" + pin +
           "\",\"confirmDisable\":" + confirmValue + "}";
}

device_platform::HttpRequest makeProvisionRequest(std::string body) {
    auto request = makeWebRequest("POST", "/api/v1/provision", std::move(body));
    request.metadata.contentType = "application/json; charset=utf-8";
    return request;
}

device_platform::HttpResponse postProvision(ComposedFixture& fixture,
                                            const std::string& body) {
    device_platform::HttpResponse response;
    const auto request = makeProvisionRequest(body);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    return response;
}

void assertError(const device_platform::HttpResponse& response,
                 std::uint16_t status, const char* error) {
    TEST_ASSERT_EQUAL_UINT16(status, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find(error) != std::string::npos);
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
}

void test_provision_codec_accepts_exactly_the_two_schemas() {
    WebProvisionDto dto;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebProvisionDecodeStatus::Success),
        static_cast<int>(decodeWebProvision(
            provisionBodyProtect(passwordUnderTest(), kPinUnderTest), dto)));
    TEST_ASSERT_TRUE(dto.mode == WebProvisionMode::Protect);
    TEST_ASSERT_TRUE(dto.password == passwordUnderTest());
    TEST_ASSERT_TRUE(dto.servicePin == kPinUnderTest);

    WebProvisionDto disable;
    disable.password = "stale";
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebProvisionDecodeStatus::Success),
                          static_cast<int>(decodeWebProvision(
                              provisionBodyDisable(kPinUnderTest), disable)));
    TEST_ASSERT_TRUE(disable.mode == WebProvisionMode::Disable);
    TEST_ASSERT_TRUE(disable.password.empty());
    TEST_ASSERT_TRUE(disable.servicePin == kPinUnderTest);

    // Empty and short strings are well-formed: the domain judges them.
    WebProvisionDto weak;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebProvisionDecodeStatus::Success),
                          static_cast<int>(decodeWebProvision(
                              provisionBodyProtect("", ""), weak)));
}

void test_provision_codec_body_limit_and_whitespace() {
    WebProvisionDto dto;
    const auto valid = provisionBodyProtect(passwordUnderTest(), kPinUnderTest);
    std::string exact = valid;
    exact.append(kMaximumWebProvisionBodyBytes - exact.size(), ' ');
    TEST_ASSERT_EQUAL_UINT(kMaximumWebProvisionBodyBytes, exact.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebProvisionDecodeStatus::Success),
                          static_cast<int>(decodeWebProvision(exact, dto)));
    std::string over = exact + " ";
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebProvisionDecodeStatus::TooLarge),
                          static_cast<int>(decodeWebProvision(over, dto)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebProvisionDecodeStatus::Invalid),
                          static_cast<int>(decodeWebProvision("", dto)));
}

void test_provision_codec_rejects_every_schema_violation_without_partial_dto() {
    const std::string pin = kPinUnderTest;
    const std::string secret = passwordUnderTest();
    const std::string embeddedNul = std::string("{\"mode\":\"disable\",") +
                                    "\"servicePin\":\"12" + '\0' +
                                    "34\",\"confirmDisable\":true}";
    const std::string bodies[] = {
        "{}",
        "[]",
        "null",
        "\"protect\"",
        "{\"mode\":\"other\",\"servicePin\":\"" + pin + "\"}",
        "{\"mode\":\"protect\",\"password\":\"" + secret +
            "\",\"servicePin\":\"" + pin + "\",\"extra\":1}",
        "{\"mode\":\"protect\",\"mode\":\"protect\",\"password\":\"" + secret +
            "\",\"servicePin\":\"" + pin + "\"}",
        "{\"password\":\"" + secret + "\",\"servicePin\":\"" + pin + "\"}",
        "{\"mode\":\"protect\",\"password\":\"" + secret + "\"}",
        "{\"mode\":\"protect\",\"servicePin\":\"" + pin + "\"}",
        "{\"mode\":\"protect\",\"password\":\"" + secret +
            "\",\"servicePin\":\"" + pin + "\",\"confirmDisable\":true}",
        "{\"mode\":\"disable\",\"password\":\"" + secret +
            "\",\"servicePin\":\"" + pin + "\",\"confirmDisable\":true}",
        "{\"mode\":\"disable\",\"servicePin\":\"" + pin + "\"}",
        provisionBodyDisable(pin, "false"),
        provisionBodyDisable(pin, "\"true\""),
        provisionBodyDisable(pin, "1"),
        "{\"mode\":\"protect\",\"password\":1,\"servicePin\":\"" + pin + "\"}",
        "{\"mode\":\"protect\",\"password\":\"" + secret +
            "\",\"servicePin\":1234}",
        "{\"mode\":1,\"servicePin\":\"" + pin + "\"}",
        provisionBodyProtect(secret, pin) + " trailing",
        "{\"mode\":\"disable\",\"servicePin\":\"12\\u0000\",\"confirmDisable\":"
        "true}",
        embeddedNul,
    };
    for (const auto& body : bodies) {
        const std::string sentinelText(12U, 's');
        WebProvisionDto dto;
        dto.mode = WebProvisionMode::Disable;
        dto.password = sentinelText;
        dto.servicePin = "9999";
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WebProvisionDecodeStatus::Invalid),
            static_cast<int>(decodeWebProvision(body, dto)));
        TEST_ASSERT_TRUE(dto.mode == WebProvisionMode::Disable);
        TEST_ASSERT_TRUE(dto.password == sentinelText);
        TEST_ASSERT_TRUE(dto.servicePin == "9999");
    }
}

void test_provision_route_request_security_contract() {
    ComposedFixture fixture;
    const auto body = provisionBodyProtect(passwordUnderTest(), kPinUnderTest);
    device_platform::HttpResponse response;

    auto request = makeWebRequest("GET", "/api/v1/provision");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 405U, "method-not-allowed");

    // Invalid request metadata (oversized Host header) is rejected by the
    // shared metadata validation.
    request = makeProvisionRequest(body);
    request.metadata.host = std::string(1000U, 'a');
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 400U, "invalid-request");

    request = makeProvisionRequest(body);
    request.metadata.contentType = "text/plain";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 415U, "unsupported-media-type");

    request = makeProvisionRequest(body);
    request.metadata.contentType.reset();
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 415U, "unsupported-media-type");

    request = makeProvisionRequest(body);
    request.metadata.origin = "http://other.local";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 403U, "origin-rejected");

    request = makeProvisionRequest(body);
    request.metadata.origin = "null";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 403U, "origin-rejected");

    request = makeProvisionRequest(body);
    request.metadata.secFetchSite = "cross-site";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 403U, "origin-rejected");

    // A valid Origin proceeds (here: no local release, so "not allowed").
    request = makeProvisionRequest(body);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 403U, "provisioning-not-allowed");

    // Referer fallback when Origin is absent.
    request = makeProvisionRequest(body);
    request.metadata.origin.reset();
    request.metadata.referer = "http://fermenter.local/";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 403U, "provisioning-not-allowed");

    request = makeProvisionRequest(std::string(2000U, ' '));
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 413U, "request-too-large");

    request = makeProvisionRequest("{\"mode\":\"protect\"}");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    assertError(response, 400U, "invalid-json");
}

void test_provision_route_without_window_changes_nothing() {
    ComposedFixture fixture;
    const auto recordsBefore = authRecordBytes(fixture);
    const auto callsBefore = fixture.kdf.calls();
    const auto response = postProvision(
        fixture, provisionBodyProtect(passwordUnderTest(), kPinUnderTest));
    assertError(response, 403U, "provisioning-not-allowed");
    TEST_ASSERT_EQUAL_UINT(callsBefore, fixture.kdf.calls());
    TEST_ASSERT_TRUE(authRecordBytes(fixture) == recordsBefore);
}

void test_provision_route_protect_success_creates_no_session() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    const auto response = postProvision(
        fixture, provisionBodyProtect(passwordUnderTest(), kPinUnderTest));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(response.contentType == "application/json; charset=utf-8");
    TEST_ASSERT_TRUE(
        completeBody(response) ==
        "{\"provisioned\":true,\"passwordProtection\":\"enabled\"}");
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    TEST_ASSERT_TRUE(completeBody(response).find("csrf") == std::string::npos);
    TEST_ASSERT_TRUE(completeBody(response).find(passwordUnderTest()) ==
                     std::string::npos);
    TEST_ASSERT_TRUE(completeBody(response).find(kPinUnderTest) ==
                     std::string::npos);
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordProtected);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 0U));

    // Normal login works afterwards; a repeat is a conflict.
    const auto session = loginComposed(fixture);
    TEST_ASSERT_FALSE(session.cookie.empty());
    const auto repeat = postProvision(
        fixture, provisionBodyProtect(passwordUnderTest(), kPinUnderTest));
    assertError(repeat, 409U, "already-provisioned");
}

void test_provision_route_disable_success_creates_no_session() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    const auto response =
        postProvision(fixture, provisionBodyDisable(kPinUnderTest));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(
        completeBody(response) ==
        "{\"provisioned\":true,\"passwordProtection\":\"disabled\"}");
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordDisabled);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 0U));

    // The existing disabled-mode shell path still issues the anonymous session.
    auto shell = makeWebRequest("GET", "/");
    device_platform::HttpResponse shellResponse;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(shell, shellResponse));
    TEST_ASSERT_EQUAL_UINT16(200U, shellResponse.statusCode);
    TEST_ASSERT_TRUE(shellResponse.metadata.setCookie.has_value());

    const auto repeat =
        postProvision(fixture, provisionBodyDisable(kPinUnderTest));
    assertError(repeat, 409U, "already-provisioned");
}

void test_provision_route_credential_errors_are_422_and_retryable() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    const auto recordsBefore = authRecordBytes(fixture);
    assertError(postProvision(fixture,
                              provisionBodyProtect("too short", kPinUnderTest)),
                422U, "invalid-credentials");
    assertError(postProvision(fixture, provisionBodyProtect("", kPinUnderTest)),
                422U, "invalid-credentials");
    assertError(
        postProvision(fixture, provisionBodyProtect(passwordUnderTest(), "12")),
        422U, "invalid-credentials");
    assertError(postProvision(fixture, provisionBodyDisable("abcd")), 422U,
                "invalid-credentials");
    TEST_ASSERT_TRUE(authRecordBytes(fixture) == recordsBefore);
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::Unprovisioned);
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    TEST_ASSERT_EQUAL_UINT16(
        200U, postProvision(fixture, provisionBodyProtect(passwordUnderTest(),
                                                          kPinUnderTest))
                  .statusCode);
}

void test_provision_route_recovery_and_failure_mapping() {
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::forceRootState(
            fixture.application, AuthProvisioningState::RecoveryRequired));
        const auto calls = fixture.kdf.calls();
        assertError(
            postProvision(fixture, provisionBodyProtect(passwordUnderTest(),
                                                        kPinUnderTest)),
            503U, "recovery-required");
        TEST_ASSERT_EQUAL_UINT(calls, fixture.kdf.calls());
    }
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        fixture.kdf.setFailing(true);
        assertError(
            postProvision(fixture, provisionBodyProtect(passwordUnderTest(),
                                                        kPinUnderTest)),
            503U, "provisioning-failed");
        fixture.kdf.setFailing(false);
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
    }
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        fixture.store.setNextWriteFault(
            device_platform_test_support::SimulatedPersistentStateStore::
                WriteFault::PowerCutAfterCommitBeforeReturn);
        fixture.store.failNextReadAfterWrite();
        assertError(
            postProvision(fixture, provisionBodyProtect(passwordUnderTest(),
                                                        kPinUnderTest)),
            503U, "recovery-required");
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
    }
}

void test_provision_route_is_not_served_during_setup_flow() {
    ComposedFixture fixture;
    const auto home = fixture.application.applyNetworkMode(
        device_platform::NetworkMode::HOME_WIFI);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(home.status));
    TEST_ASSERT_TRUE(fixture.application.networkSetupFlowActive());
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());

    auto request = makeProvisionRequest(
        provisionBodyProtect(passwordUnderTest(), kPinUnderTest));
    device_platform::HttpResponse response;
    // The setup flow owns the surface: the dispatcher does not handle it.
    TEST_ASSERT_FALSE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::Unprovisioned);

    // After the network setup completed the normal route answers.
    auto candidate =
        makeWebRequest("POST", "/api/network/candidate",
                       "ssid=next-network&password=next-network-password");
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(candidate, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_FALSE(fixture.application.networkSetupFlowActive());
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::PasswordProtected);
}

device_platform::HttpRequest makeRunRequest(const std::string& cookie,
                                            const std::string& csrf,
                                            const std::string& seq,
                                            std::string body) {
    auto request =
        makeAuthenticatedRequest("POST", "/internal/ui/run", cookie, csrf);
    request.body = std::move(body);
    request.metadata.contentType = "application/json; charset=utf-8";
    request.metadata.mutationSeq = seq;
    return request;
}

void test_run_route_is_gated_by_authentication_state() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                     WebAuthenticationState::Unprovisioned);
    auto request = makeRunRequest("0", "0", "1", "{}");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).find(
                         "authentication-unavailable") != std::string::npos);
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
}

void test_run_route_is_composed_with_session_csrf_and_replay() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto session = loginComposed(fixture);
    const auto sequence =
        fixture.application.uiSnapshot().revisions.expectedStateSequence;
    const std::string body =
        "{\"v\":1,\"r\":{\"s\":" + std::to_string(sequence) +
        "},\"i\":{\"t\":\"reset-fault\"}}";

    device_platform::HttpResponse response;
    auto request = makeWebRequest("POST", "/internal/ui/run", body);
    request.metadata.contentType = "application/json; charset=utf-8";
    request.metadata.mutationSeq = "1";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);

    request = makeRunRequest(session.cookie, "wrong", "1", body);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(403U, response.statusCode);

    request = makeRunRequest(session.cookie, session.csrf, "1", body);
    request.method = "GET";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(405U, response.statusCode);

    request = makeRunRequest(session.cookie, session.csrf, "1", body);
    device_platform::HttpResponse first;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, first));
    TEST_ASSERT_TRUE(completeBody(first).find("\"outcome\"") !=
                     std::string::npos);
    device_platform::HttpResponse replay;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, replay));
    TEST_ASSERT_EQUAL_UINT16(first.statusCode, replay.statusCode);
    TEST_ASSERT_EQUAL_STRING(completeBody(first).c_str(),
                             completeBody(replay).c_str());
}

// ---- C2 (Issue #27): dispatcher-level run mutation evidence ----------------
// Reachability result: with the factory catalog and simulated owning evidence
// no program is runnable without commissioning values (start-program ->
// 422), so start/stop/complete have no positive path here. Positive evidence
// uses the RAM-owned ack/mute path; start-program is checked fail-closed.

struct ComposedRunSession {
    ComposedWebSession web;
    std::uint64_t next{1U};
};

ComposedRunSession composedRunSession(ComposedFixture& fixture) {
    if (fixture.application.webAuthenticationState() ==
        WebAuthenticationState::Unprovisioned) {
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, "correct horse battery", "1234"));
    }
    return {loginComposed(fixture), 1U};
}

std::string runBody(const FermentationApplication& application,
                    const std::string& intent) {
    return "{\"v\":1,\"r\":" +
           expectedJson(application.uiSnapshot().revisions) +
           ",\"i\":" + intent + "}";
}

std::string ackIntent(std::uint32_t id) {
    return "{\"t\":\"ack-message\",\"id\":" + std::to_string(id) + "}";
}

device_platform::HttpResponse postRun(ComposedFixture& fixture,
                                      const ComposedRunSession& session,
                                      std::uint64_t seq,
                                      const std::string& body) {
    const auto request = makeRunRequest(session.web.cookie, session.web.csrf,
                                        std::to_string(seq), body);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    return response;
}

void test_dispatcher_run_ack_and_mute_apply_once_and_replay_without_second_effect() {
    for (const bool mute : {false, true}) {
        ComposedFixture fixture;
        installMessage(fixture.application, kHandlerMessageId);
        auto session = composedRunSession(fixture);
        const auto intent =
            mute ? "{\"t\":\"mute-message\",\"id\":7}" : ackIntent(7U);
        const auto body = runBody(fixture.application, intent);
        const auto first = postRun(fixture, session, 1U, body);
        TEST_ASSERT_EQUAL_UINT16(200U, first.statusCode);
        TEST_ASSERT_EQUAL_STRING("{\"outcome\":\"applied\"}",
                                 completeBody(first).c_str());
        const auto& message =
            FermentationApplicationTestAccess::runtimeState(fixture.application)
                .messages[0];
        TEST_ASSERT_EQUAL_INT(mute ? 0 : 1, message.acknowledged ? 1 : 0);
        TEST_ASSERT_EQUAL_INT(mute ? 1 : 0, message.acousticMuted ? 1 : 0);
        const auto revisions = fixture.application.uiSnapshot().revisions;

        const auto replay = postRun(fixture, session, 1U, body);
        TEST_ASSERT_EQUAL_UINT16(first.statusCode, replay.statusCode);
        TEST_ASSERT_EQUAL_STRING(completeBody(first).c_str(),
                                 completeBody(replay).c_str());
        TEST_ASSERT_EQUAL_UINT32(
            revisions.expectedMessageRevision.value_or(0U),
            fixture.application.uiSnapshot()
                .revisions.expectedMessageRevision.value_or(0U));
    }
}

void test_dispatcher_run_two_sessions_same_revision_yield_one_effect() {
    ComposedFixture fixture;
    installMessage(fixture.application, kHandlerMessageId);
    auto first = composedRunSession(fixture);
    auto second = composedRunSession(fixture);
    const auto body = runBody(fixture.application, ackIntent(7U));

    TEST_ASSERT_EQUAL_UINT16(200U,
                             postRun(fixture, first, 1U, body).statusCode);
    const auto afterFirst = fixture.application.uiSnapshot().revisions;
    const auto loser = postRun(fixture, second, 1U, body);
    TEST_ASSERT_EQUAL_UINT16(409U, loser.statusCode);
    TEST_ASSERT_EQUAL_UINT32(
        afterFirst.expectedMessageRevision.value_or(0U),
        fixture.application.uiSnapshot()
            .revisions.expectedMessageRevision.value_or(0U));
    TEST_ASSERT_EQUAL_UINT32(
        afterFirst.expectedStateSequence,
        fixture.application.uiSnapshot().revisions.expectedStateSequence);
}

void test_dispatcher_run_local_change_between_status_and_post_is_409() {
    ComposedFixture fixture;
    installMessage(fixture.application, kHandlerMessageId);
    auto session = composedRunSession(fixture);
    const auto body = runBody(fixture.application, ackIntent(7U));
    // Touch-side change after the status read, before the POST.
    FermentationApplicationTestAccess::runtimeState(fixture.application)
        .messageRevision += 1U;
    const auto response = postRun(fixture, session, 1U, body);
    TEST_ASSERT_EQUAL_UINT16(409U, response.statusCode);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::runtimeState(fixture.application)
            .messages[0]
            .acknowledged);
}

void test_dispatcher_run_start_program_is_fail_closed_and_changes_nothing() {
    ComposedFixture fixture;
    auto session = composedRunSession(fixture);
    const auto before = fixture.application.uiSnapshot();
    const auto bodyFor = [&](const char* program,
                             const std::string& revisions) {
        return "{\"v\":1,\"r\":" + revisions +
               ",\"i\":{\"t\":\"start-program\",\"c\":{\"p\":\"" + program +
               "\"}}}";
    };
    const auto revisions = expectedJson(before.revisions);
    TEST_ASSERT_TRUE(
        before.revisions.expectedProgramCatalogRevision.has_value());

    // Unknown program and a factory program without commissioning data.
    TEST_ASSERT_EQUAL_UINT16(
        422U, postRun(fixture, session, 1U, bodyFor("not-installed", revisions))
                  .statusCode);
    TEST_ASSERT_EQUAL_UINT16(
        422U, postRun(fixture, session, 2U, bodyFor("yogurt-mild", revisions))
                  .statusCode);
    // Stale catalog revision.
    const auto stale =
        "{\"s\":" + std::to_string(before.revisions.expectedStateSequence) +
        ",\"c\":\"" +
        std::to_string(
            before.revisions.expectedProgramCatalogRevision->value() + 1U) +
        "\"}";
    TEST_ASSERT_EQUAL_UINT16(
        409U, postRun(fixture, session, 3U, bodyFor("yogurt-mild", stale))
                  .statusCode);

    const auto after = fixture.application.uiSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::Standby),
                          static_cast<int>(after.home.mode));
    TEST_ASSERT_EQUAL_UINT32(before.revisions.expectedStateSequence,
                             after.revisions.expectedStateSequence);
    const auto head = fixture.store.read(runHeadKey(), 8240U);
    TEST_ASSERT_TRUE(head.status !=
                     device_platform::StateStoreReadStatus::Success);
}

void test_dispatcher_run_limits_gated_actions_stay_503() {
    ComposedFixture fixture;
    auto session = composedRunSession(fixture);
    const std::string plan =
        "{\"x\":8.0,\"s\":\"air\",\"h\":false,\"q\":0.5,\"qd\":10,"
        "\"tr\":60}";
    const std::array<std::string, 3U> intents{
        "{\"t\":\"start-manual-holding\",\"p\":" + plan + "}",
        "{\"t\":\"stop-run\",\"o\":\"abort-and-cool\",\"p\":" + plan + "}",
        "{\"t\":\"complete-run\",\"c\":true,\"p\":" + plan + "}"};
    std::uint64_t seq = 1U;
    for (const auto& intent : intents) {
        const auto response = postRun(fixture, session, seq++,
                                      runBody(fixture.application, intent));
        TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    }
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::Standby),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

void test_dispatcher_run_mutation_sequence_contract() {
    ComposedFixture fixture;
    installMessage(fixture.application, kHandlerMessageId);
    auto a = composedRunSession(fixture);
    auto b = composedRunSession(fixture);
    const auto body = runBody(fixture.application, ackIntent(7U));

    // Gap: no application call, no consumption.
    TEST_ASSERT_EQUAL_UINT16(409U, postRun(fixture, a, 3U, body).statusCode);
    // Dispatcher-level rejections that must not consume the sequence.
    TEST_ASSERT_EQUAL_UINT16(400U, postRun(fixture, a, 1U, "{").statusCode);
    {
        auto request = makeRunRequest(a.web.cookie, "wrong", "1", body);
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(403U, response.statusCode);
        request = makeRunRequest(a.web.cookie, a.web.csrf, "1", body);
        request.metadata.contentType = "text/plain";
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(415U, response.statusCode);
        request = makeRunRequest(a.web.cookie, a.web.csrf, "1",
                                 std::string(600U, 'x'));
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(413U, response.statusCode);
        TEST_ASSERT_TRUE(completeBody(response).find("request-too-large") !=
                         std::string::npos);
    }

    const auto unknown =
        runBody(fixture.application,
                "{\"t\":\"start-program\",\"c\":{\"p\":\"not-installed\"}}");
    // The untouched sequence 1 is still the next one and gets consumed.
    TEST_ASSERT_EQUAL_UINT16(200U, postRun(fixture, a, 1U, body).statusCode);
    // Reused with another body, and a gap after consumption.
    const auto reused = postRun(fixture, a, 1U, unknown);
    TEST_ASSERT_EQUAL_UINT16(409U, reused.statusCode);
    TEST_ASSERT_TRUE(reused.body.find("sequence-conflict") !=
                     std::string::npos);
    // A 422 application outcome consumes the sequence; sessions are
    // independent.
    TEST_ASSERT_EQUAL_UINT16(422U, postRun(fixture, a, 2U, unknown).statusCode);
    TEST_ASSERT_EQUAL_UINT16(422U, postRun(fixture, b, 1U, unknown).statusCode);
    // Same sequence and body replays the stored outcome; a gap is refused.
    TEST_ASSERT_EQUAL_UINT16(422U, postRun(fixture, a, 2U, unknown).statusCode);
    TEST_ASSERT_EQUAL_UINT16(409U, postRun(fixture, a, 5U, unknown).statusCode);
    // Replay window: after eight further outcomes sequence 2 has expired.
    for (std::uint64_t seq = 3U; seq <= 10U; ++seq) {
        TEST_ASSERT_EQUAL_UINT16(422U,
                                 postRun(fixture, a, seq, unknown).statusCode);
    }
    TEST_ASSERT_EQUAL_UINT16(409U, postRun(fixture, a, 2U, unknown).statusCode);
}

// ---- S6: first-time setup form in the web shell -----------------------------

std::string rawShell(ComposedFixture& fixture, const std::string& cookie = {}) {
    auto request = makeWebRequest("GET", "/");
    if (!cookie.empty()) request.metadata.cookie = "FSSESSION=" + cookie;
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    return completeBody(response);
}

std::string shellBody(ComposedFixture& fixture) {
    auto request = makeWebRequest("GET", "/");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    return shellDocument(response);
}

bool contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

size_t occurrences(const std::string& text, const std::string& needle) {
    size_t count = 0U;
    for (size_t at = text.find(needle); at != std::string::npos;
         at = text.find(needle, at + needle.size())) {
        ++count;
    }
    return count;
}

// Owner limit of the web shell (Issue #27 language-selection slice, D1 = A:
// at most 11456 bytes, one time) and the required reserve below it, including
// the 19 byte reserve for the widest mutation sequence.
constexpr size_t kShellLimit = 4096U;
constexpr size_t kShellMinimumReserve = 128U;

void test_unprovisioned_shell_shows_the_setup_form_within_the_size_limit() {
    ComposedFixture fixture;
    const auto body = shellBody(fixture);
    TEST_ASSERT_TRUE(body.size() <= kShellLimit);
    // Protect is preselected; password, separate Service-PIN and the explicit
    // disable confirmation are separate controls.
    TEST_ASSERT_TRUE(contains(body, "name=\"o\" value=\"p\" checked"));
    TEST_ASSERT_TRUE(contains(body, "name=\"o\" value=\"d\">"));
    TEST_ASSERT_TRUE(contains(body, "id=\"w\" type=\"password\""));
    TEST_ASSERT_TRUE(contains(body, "id=\"n\" type=\"password\""));
    TEST_ASSERT_TRUE(contains(body, "id=\"k\" type=\"checkbox\""));
    // The warning and the confirmation are hidden until "disable" is chosen.
    TEST_ASSERT_TRUE(contains(body, "<p id=\"x\" hidden>"));
    TEST_ASSERT_TRUE(contains(body, "<label id=\"lk\" hidden>"));
    TEST_ASSERT_TRUE(contains(body, "q('x').hidden=q('lk').hidden=!d()"));
    // No login, session, status polling or logout in this state.
    TEST_ASSERT_FALSE(contains(body, "/api/v1/login"));
    TEST_ASSERT_FALSE(contains(body, "/api/v1/logout"));
    TEST_ASSERT_FALSE(contains(body, "/api/v1/status"));
    TEST_ASSERT_FALSE(contains(body, "csrf"));
    TEST_ASSERT_FALSE(contains(body, "__WEB_CSRF__"));
}

void test_unprovisioned_shell_requests_match_the_s4_contract() {
    ComposedFixture fixture;
    const auto body = shellBody(fixture);
    TEST_ASSERT_TRUE(contains(body, "fetch('/api/v1/provision'"));
    TEST_ASSERT_TRUE(contains(body, "method:'POST'"));
    TEST_ASSERT_TRUE(contains(body, "'Content-Type':'application/json'"));
    // Key set and order exactly as the S4 codec expects them.
    TEST_ASSERT_TRUE(contains(body,
                              "{mode:'disable',servicePin:q('n').value,"
                              "confirmDisable:true}"));
    TEST_ASSERT_TRUE(contains(body,
                              "{mode:'protect',password:q('w').value,"
                              "servicePin:q('n').value}"));
    // Disable is only sent after the explicit confirmation is ticked.
    TEST_ASSERT_TRUE(contains(body, "if(d()&&!q('k').checked)"));

    // The exact bodies the page builds are accepted by the S4 route/codec.
    WebProvisionDto dto;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebProvisionDecodeStatus::Success),
        static_cast<int>(decodeWebProvision(
            "{\"mode\":\"protect\",\"password\":\"x\",\"servicePin\":\"1\"}",
            dto)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(WebProvisionDecodeStatus::Success),
                          static_cast<int>(decodeWebProvision(
                              "{\"mode\":\"disable\",\"servicePin\":\"1\","
                              "\"confirmDisable\":true}",
                              dto)));
}

void test_unprovisioned_shell_explains_local_release_and_creates_no_session() {
    ComposedFixture fixture;
    const auto body = shellBody(fixture);
    // 403 is explained as a local release on the device; 409/422/503 have
    // their own messages; nothing logs in or creates a session afterwards.
    // The local-release hint is bound to the error code, not to every 403
    // (a rejected origin must not send the user to the device).
    TEST_ASSERT_TRUE(
        contains(body, "j.error==='provisioning-not-allowed'?'na'"));
    TEST_ASSERT_FALSE(contains(body, "r.status===403"));
    // Every message goes through sm(): the page keeps the message key and
    // re-renders it from the table when the language changes.
    TEST_ASSERT_TRUE(
        contains(body, "function sm(k){M=k;q('m').textContent=T[k];}"));
    TEST_ASSERT_TRUE(contains(body, "q('m').textContent=M?T[M]:'';"));
    TEST_ASSERT_FALSE(contains(body, "q('m').textContent=T."));
    TEST_ASSERT_TRUE(contains(body, "r.status===409?'ap'"));
    TEST_ASSERT_TRUE(contains(body, "r.status===422?'ic'"));
    TEST_ASSERT_TRUE(contains(body, "j.error==='recovery-required'?'rc'"));
    TEST_ASSERT_TRUE(contains(body, "release web setup on the device first"));
    TEST_ASSERT_TRUE(contains(body, "am Geraet freigeben"));
    TEST_ASSERT_TRUE(contains(body, "en el equipo"));
    TEST_ASSERT_FALSE(contains(body, "location.reload"));
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 0U));
}

void test_unprovisioned_shell_texts_are_complete_in_all_locales() {
    ComposedFixture fixture;
    const auto body = shellBody(fixture);
    for (const char* key : {"a", "b", "c", "e", "x", "y", "g", "ok", "na", "ic",
                            "ap", "rc", "fl"}) {
        // Every message key exists exactly once per locale (de, en, es).
        TEST_ASSERT_EQUAL_UINT(
            3U, occurrences(body, std::string(1U, ',') + key + ":'") +
                    occurrences(body, std::string(1U, '{') + key + ":'"));
    }
    TEST_ASSERT_TRUE(contains(body, "de:{"));
    TEST_ASSERT_TRUE(contains(body, ",en:{"));
    TEST_ASSERT_TRUE(contains(body, ",es:{"));
    // The language is chosen by the validated code (plan a671c33), not by any
    // two letter table key.
    TEST_ASSERT_TRUE(contains(body, "ok=/^(de|en|es)$/"));
    TEST_ASSERT_TRUE(contains(body, "T=L[c]"));
    TEST_ASSERT_FALSE(contains(body, "L[g]"));
}

std::string authenticatedShell(ComposedFixture& fixture,
                               const std::string& cookie) {
    auto request = makeAuthenticatedRequest("GET", "/", cookie);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    return shellDocument(response);
}

size_t labelCount(const std::string& body, const std::string& token);

void test_shell_run_controls_text_contract() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, passwordUnderTest(), kPinUnderTest));
    const auto body = shellBody(fixture);
    // One button per action; no window.confirm and no extra endpoint.
    for (const char* action : {"ack", "mute", "stop", "done", "start"}) {
        TEST_ASSERT_EQUAL_UINT32(
            1U, occurrences(body, std::string("data-a=\"") + action + "\""));
    }
    TEST_ASSERT_FALSE(contains(body, "confirm("));
    // The only browser storage is the validated language code (plan a671c33):
    // one read and one guarded write of localStorage.lang; no cookie and no
    // session storage.
    TEST_ASSERT_EQUAL_UINT32(2U, occurrences(body, "localStorage"));
    TEST_ASSERT_EQUAL_UINT32(2U, occurrences(body, "localStorage.lang"));
    TEST_ASSERT_FALSE(contains(body, "sessionStorage"));
    TEST_ASSERT_FALSE(contains(body, "document.cookie"));
    TEST_ASSERT_EQUAL_UINT32(1U, occurrences(body, "/internal/ui/run"));
    TEST_ASSERT_TRUE(contains(body, "X-UI-Mutation-Seq"));
    TEST_ASSERT_FALSE(contains(body, "X-Mutation-Seq"));
    TEST_ASSERT_TRUE(contains(body, "X-CSRF-Token"));
    // Ack/Mute are sent directly; Stop, Done and Start are armed first (D1).
    TEST_ASSERT_TRUE(contains(body, "if(a==='ack'||a==='mute'){"));
    TEST_ASSERT_TRUE(contains(body, "if(!cf(a))return;"));
    TEST_ASSERT_TRUE(contains(body, "T.sure"));
    // Intents match the fixed wire contract.
    TEST_ASSERT_TRUE(contains(body, "t:a+'-message'"));
    TEST_ASSERT_TRUE(contains(body, "{t:'stop-run',o:'abort-and-turn-off'}"));
    TEST_ASSERT_TRUE(contains(body, "{t:'complete-run',c:false}"));
    TEST_ASSERT_TRUE(contains(body, "{t:'start-program',c:sc()}"));
    // Closed outcomes; an unavailable/indeterminate result locks the
    // sequence and refreshes state, never implies a successful commit.
    for (const auto* token : {"applied:", "stale:", "busy:", "rejected:",
                              "'confirmation-required':", "'too-large':",
                              "'write-failed':", "unavailable:"})
        TEST_ASSERT_TRUE(contains(body, token));
    TEST_ASSERT_TRUE(contains(body, "location.reload()"));
    TEST_ASSERT_TRUE(contains(body, "Q=j.outcome==='unavailable'?0:n+1"));
    TEST_ASSERT_TRUE(contains(body, "t<2&&!r"));
    // All locales carry the confirmation and result texts.
    TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, "sure"));
    TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, "ok"));
    TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, "fail"));
}

// String/state contract only: the native tools cannot execute the shell script
// (a browser run stays NOT_RUN). The application remains the decisive gate.
void test_shell_action_availability_and_confirmation_binding_contract() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, passwordUnderTest(), kPinUnderTest));
    const auto body = shellBody(fixture);
    // Availability per home mode: Stop only for active-run/waiting, Complete
    // only for completed, Start only for standby with a selected program.
    TEST_ASSERT_TRUE(contains(body, "r=h==='active-run'||h==='waiting'"));
    TEST_ASSERT_TRUE(contains(body,
                              "stop:!r,done:h!=='completed',"
                              "start:ps.disabled"));
    // A pending confirmation is bound to mode, run and catalog revision and
    // the selected program; it is dropped on change, on send and without a
    // valid snapshot. Unchanged polls keep it.
    TEST_ASSERT_TRUE(contains(body,
                              "c=P?[h,rv().run,rv()[Z],PC,pg.value,JSON."
                              "stringify(sc()),SD?.revision,sn.value]"
                              ".join():''"));
    TEST_ASSERT_TRUE(contains(body, "if(!P||KC&&c!==KC)K='';KC=K?KC||c:'';"));
    TEST_ASSERT_TRUE(contains(body, "pg.onchange=()=>ld();"));
    TEST_ASSERT_TRUE(contains(body, "send(i){K='';B=true;"));
    // A failed refresh makes the snapshot unusable until a fresh one arrives.
    // Abort the siblings, then check the generation before display effects
    // (Issue #27 dashboard slice); an applied failure then clears P, updates
    // the action lock and the display.
    {
        const auto handler = body.find("const e=()=>{c.abort();");
        TEST_ASSERT_TRUE(handler != std::string::npos);
        const auto guard = body.find("if(my===G){", handler);
        const auto clear = body.find("P=null;", handler);
        const auto lock = body.find("ui();", clear);
        TEST_ASSERT_TRUE(guard != std::string::npos);
        TEST_ASSERT_TRUE(guard < clear);
        TEST_ASSERT_TRUE(clear < lock);
    }
    TEST_ASSERT_TRUE(contains(body, "b.disabled=B||!P||d[k]"));
    // Ack/Mute name their target message.
    TEST_ASSERT_TRUE(contains(body, "T[k]+(g[k]?' #'+g[k].id:'')"));
}

void test_shell_embeds_mutation_sequence_per_session() {
    ComposedFixture fixture;
    installMessage(fixture.application, kHandlerMessageId);
    auto a = composedRunSession(fixture);
    auto b = composedRunSession(fixture);

    // Anonymous page of a protected device: no session, no sequence.
    TEST_ASSERT_TRUE(contains(shellBody(fixture), "const U=false;let Q=0;"));

    TEST_ASSERT_TRUE(contains(authenticatedShell(fixture, a.web.cookie),
                              "const U=true;let Q=1;"));
    // Reading the shell neither consumes nor advances the sequence.
    TEST_ASSERT_TRUE(
        contains(authenticatedShell(fixture, a.web.cookie), "let Q=1;"));

    const auto body =
        runBody(fixture.application, ackIntent(kHandlerMessageId));
    TEST_ASSERT_EQUAL_UINT16(200U, postRun(fixture, a, 1U, body).statusCode);
    TEST_ASSERT_TRUE(
        contains(authenticatedShell(fixture, a.web.cookie), "let Q=2;"));
    // The second session keeps its own counter.
    TEST_ASSERT_TRUE(
        contains(authenticatedShell(fixture, b.web.cookie), "let Q=1;"));
}

void test_shell_login_reload_bootstrap_for_protected_and_disabled() {
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        TEST_ASSERT_TRUE(contains(shellBody(fixture), "let Q=0;"));
        const auto session = loginComposed(fixture);
        TEST_ASSERT_TRUE(contains(authenticatedShell(fixture, session.cookie),
                                  "const U=true;let Q=1;"));
        TEST_ASSERT_TRUE(contains(shellBody(fixture), "lg.addEventListener"));
        TEST_ASSERT_TRUE(
            contains(shellBody(fixture), "if(r.ok)location.reload();"));
    }
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::setWebPasswordEnabled(
                fixture.application, false));
        auto request = makeWebRequest("GET", "/");
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_TRUE(response.metadata.setCookie.has_value());
        TEST_ASSERT_TRUE(contains(completeBody(response).c_str(), "let Q=1;"));
        const auto cookie = sessionCookieValue(response);
        TEST_ASSERT_TRUE(
            contains(authenticatedShell(fixture, cookie), "let Q=1;"));
    }
}

void test_shell_stays_within_the_limit_in_all_states() {
    constexpr size_t kLimit = 2048U;
    // Reserve for the widest possible sequence (20 digits) on top of "1".
    constexpr size_t kSequenceHeadroom = 19U;
    {
        ComposedFixture fixture;  // unprovisioned setup form
        TEST_ASSERT_TRUE(rawShell(fixture).size() + kShellMinimumReserve <=
                         4096U);
    }
    {
        ComposedFixture fixture;  // protected, anonymous and authenticated
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        TEST_ASSERT_TRUE(rawShell(fixture).size() <= kLimit);
        const auto session = loginComposed(fixture);
        const auto measuredBytes = rawShell(fixture, session.cookie).size();
        std::printf("WEB_SHELL_AUTH_BYTES=%zu RESERVE=%zu\n", measuredBytes,
                    kLimit - measuredBytes);
        TEST_ASSERT_TRUE(rawShell(fixture, session.cookie).size() +
                             kSequenceHeadroom <=
                         kLimit);
        // The measured shell keeps the owner reserve (it includes the
        // sequence headroom above).
        TEST_ASSERT_TRUE(rawShell(fixture, session.cookie).size() +
                             kShellMinimumReserve <=
                         kLimit);
    }
    {
        ComposedFixture fixture;  // password disabled (anonymous session)
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::setWebPasswordEnabled(
                fixture.application, false));
        auto request = makeWebRequest("GET", "/");
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
        std::printf("WEB_SHELL_DISABLED_BYTES=%zu RESERVE=%zu\n",
                    completeBody(response).size(),
                    kLimit - completeBody(response).size());
        TEST_ASSERT_TRUE(completeBody(response).size() + kSequenceHeadroom <=
                         kLimit);
        TEST_ASSERT_TRUE(completeBody(response).size() + kShellMinimumReserve <=
                         kLimit);
    }
    {
        ComposedFixture fixture;  // recovery required
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::forceRootState(
            fixture.application, AuthProvisioningState::RecoveryRequired));
        TEST_ASSERT_TRUE(rawShell(fixture).size() <= kLimit);
    }
}

// ---- Issue #27 dashboard slice: SIM-27-W-01..06
// ------------------------------ String/state contract only: the native tools
// cannot execute the shell script. Dynamic browser behaviour (layout) is
// NOT_RUN. scripts/test_web_shell_polling.mjs executes the actual JS with fake
// fetch/DOM/time for serialisation, abort and post-mutation locks; the native
// checks below retain the string/state contract.

std::string loggedInShell(ComposedFixture& fixture) {
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, passwordUnderTest(), kPinUnderTest));
    const auto session = loginComposed(fixture);
    return authenticatedShell(fixture, session.cookie);
}

// Text of the refresh function (from its declaration to the next top-level
// function) so position checks cannot match other code.
std::string refreshFunction(const std::string& body) {
    const auto begin = body.find("function rf(f=0){");
    TEST_ASSERT_TRUE(begin != std::string::npos);
    const auto end = body.find("lg.addEventListener", begin);
    TEST_ASSERT_TRUE(end != std::string::npos);
    return body.substr(begin, end - begin);
}

std::string uiFunction(const std::string& body) {
    const auto begin = body.find("function ui(){");
    TEST_ASSERT_TRUE(begin != std::string::npos);
    const auto end = body.find("async function send(", begin);
    TEST_ASSERT_TRUE(end != std::string::npos);
    return body.substr(begin, end - begin);
}

// C1: each dictionary key exists exactly once as a de/en/es value array
// (key:["de","en","es"]). Returns the number of values of that unique entry,
// 0 when the key is missing or duplicated.
size_t labelCount(const std::string& body, const std::string& token) {
    const bool plain = token.find('-') == std::string::npos;
    const auto key = plain ? token : "\"" + token + "\"";
    const auto needle = key + ":[";
    if (occurrences(body, "{" + needle) + occurrences(body, "," + needle) !=
        1U) {
        return 0U;
    }
    auto at = body.find("{" + needle);
    if (at == std::string::npos) at = body.find("," + needle);
    size_t values = 0U;
    bool quoted = false;
    for (size_t i = at + needle.size() + 1U; i < body.size(); ++i) {
        if (body[i] == '\\' && quoted) {
            ++i;
        } else if (body[i] == '"') {
            if (quoted) ++values;
            quoted = !quoted;
        } else if (body[i] == ']' && !quoted) {
            break;
        }
    }
    return values;
}

std::vector<std::string> jsonTokens(const std::string& json,
                                    const std::string& key) {
    std::vector<std::string> tokens;
    const auto needle = "\"" + key + "\":\"";
    for (auto at = json.find(needle); at != std::string::npos;
         at = json.find(needle, at + needle.size())) {
        const auto start = at + needle.size();
        tokens.push_back(json.substr(start, json.find('"', start) - start));
    }
    return tokens;
}

void test_dashboard_shell_structure_and_responsive_contract() {
    ComposedFixture fixture;
    const auto body = loggedInShell(fixture);
    // SIM-27-W-01: readable board instead of the raw JSON dump.
    TEST_ASSERT_FALSE(contains(body, "JSON.stringify(p"));
    TEST_ASSERT_FALSE(contains(body, "<pre"));
    TEST_ASSERT_FALSE(contains(body, "innerHTML"));
    for (const char* id : {"connection", "mode", "temps", "alerts", "board"}) {
        TEST_ASSERT_EQUAL_UINT32(
            1U, occurrences(body, std::string("id=\"") + id + "\""));
    }
    TEST_ASSERT_EQUAL_UINT32(1U, occurrences(body, "name=\"viewport\""));
    TEST_ASSERT_EQUAL_UINT32(1U, occurrences(body, "@media(min-width:40rem)"));
    TEST_ASSERT_TRUE(contains(body, "min-height:2.75rem"));
    // API strings reach the DOM only through textContent/className.
    TEST_ASSERT_TRUE(contains(body, "X='textContent'"));
    TEST_ASSERT_TRUE(contains(body, "e[X]=x;"));
    // The existing controls, session embedding and polling are unchanged.
    for (const char* action : {"ack", "mute", "stop", "done", "start"}) {
        TEST_ASSERT_EQUAL_UINT32(
            1U, occurrences(body, std::string("data-a=\"") + action + "\""));
    }
    TEST_ASSERT_TRUE(contains(body, "const U=true;let Q=1;"));
    TEST_ASSERT_EQUAL_UINT32(1U, occurrences(body, "const A='"));
    // SIM-27-W-06: no new endpoint, three read-only requests, 5 s polling.
    TEST_ASSERT_EQUAL_UINT32(1U, occurrences(body, "setInterval("));
    TEST_ASSERT_TRUE(contains(body, "setInterval(rf,5000)"));
    TEST_ASSERT_TRUE(contains(body,
                              "['status','temperatures','alerts'].map(async "
                              "x=>{const a=await fetch('/api/v1/'+x,"));
    TEST_ASSERT_EQUAL_UINT32(5U, occurrences(body, "fetch('"));
}

void test_dashboard_shell_texts_are_complete_in_all_locales() {
    ComposedFixture fixture;
    const auto body = loggedInShell(fixture);
    // SIM-27-W-02: every dictionary key exists exactly once per locale.
    for (const char* key :
         {"login",      "logout",   "offline",     "stale",   "online",
          "rq",         "pl",       "pt",          "ds",      "ack",
          "mute",       "stop",     "done",        "start",   "sure",
          "ok",         "fail",     "age",         "failed",  "alerts",
          "qd",         "sm",       "standby",     "waiting", "completed",
          "restricted", "recovery", "unavailable", "product", "cooling"}) {
        TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, key));
    }
    for (const char* key : {"active-run", "cabinet-air"}) {
        TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, key));
    }
    // Value order de, en, es; English fallback is kept (lc falls back to 'en').
    TEST_ASSERT_TRUE(contains(body, "{de:0,en:1,es:2}[lc]"));
    TEST_ASSERT_TRUE(contains(body, "ok=/^(de|en|es)$/"));
    TEST_ASSERT_TRUE(contains(body, "lc=ok.test(c)?c:'en'"));
    // The warning text of the disabled mode stays as before.
    TEST_ASSERT_TRUE(contains(body, "Password protection is disabled"));
}

// C3: the shell reads exactly the run members the codec writes, and every
// token the codec can emit for the run information has a label in all locales.
void test_shell_run_information_is_bound_to_the_status_codec() {
    ComposedFixture fixture;
    const auto body = loggedInShell(fixture);

    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::ActiveRun;
    snapshot.home.activeRunId = "run-1";
    snapshot.home.programName = "Brot";
    FermentationRunDetail detail;
    detail.sourceKind = ProgramSourceKind::UserProgram;
    detail.targetCelsius = 24.5;
    detail.completion =
        RunDetailCompletion{CompletionMode::CoolAndHoldForDuration, 8.0, 30U};
    detail.time.elapsedSeconds = 61U;
    detail.time.remainingSeconds = 7139U;
    detail.time.remainingKind = RunDetailRemainingKind::Timed;
    snapshot.home.runDetail = detail;
    std::string status;
    TEST_ASSERT_TRUE(
        encodeWebApiStatus(snapshot, FermentationDeviceInfoView{}, status));
    for (const char* member :
         {"programName", "sourceKind", "targetCelsius", "completion",
          "coolingTargetCelsius", "holdDurationMinutes", "elapsedSeconds",
          "remainingSeconds", "remainingKind", "scope"}) {
        TEST_ASSERT_TRUE(
            contains(status, (std::string("\"") + member + "\":").c_str()));
        TEST_ASSERT_TRUE(contains(body, member));
    }
    TEST_ASSERT_TRUE(contains(body, "a.run"));

    std::set<std::string> modes;
    std::set<std::string> kinds;
    std::set<std::string> states;
    for (unsigned value = 0U; value < 4U; ++value) {
        detail.completion->mode = static_cast<CompletionMode>(value);
        for (unsigned kind = 0U; kind < 4U; ++kind) {
            detail.time.remainingKind =
                static_cast<RunDetailRemainingKind>(kind);
            snapshot.home.runDetail = detail;
            std::string encoded;
            TEST_ASSERT_TRUE(encodeWebApiStatus(
                snapshot, FermentationDeviceInfoView{}, encoded));
            for (const auto& token : jsonTokens(encoded, "mode")) {
                modes.insert(token);
            }
            for (const auto& token : jsonTokens(encoded, "remainingKind")) {
                kinds.insert(token);
            }
        }
    }
    for (unsigned value = 0U; value < 32U; ++value) {
        FermentationUiSnapshot plain;
        plain.home.processState = static_cast<ProcessState>(value);
        std::string encoded;
        if (encodeWebApiStatus(plain, FermentationDeviceInfoView{}, encoded)) {
            for (const auto& token : jsonTokens(encoded, "processState")) {
                states.insert(token);
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(4U, static_cast<std::uint32_t>(modes.size()));
    TEST_ASSERT_EQUAL_UINT32(4U, static_cast<std::uint32_t>(kinds.size()));
    TEST_ASSERT_EQUAL_UINT32(15U, static_cast<std::uint32_t>(states.size()));
    for (const auto& token : modes) {
        TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, "c-" + token));
    }
    for (const auto& token : states) {
        TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, "p-" + token));
    }
    // Timed and unknown need no text of their own; the other kinds do.
    TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, "unbounded"));
    TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, "not-applicable"));
    TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, "manual-timed"));
}

// Installs a state-conforming active run (frozen program with a name of the
// 96 byte maximum, 48 byte run id, Fermenting with a consistent run snapshot)
// through the application's own runtime state, so the status handler projects
// it through the real application path.
void installActiveRunForStatus(FermentationApplication& application,
                               const std::string& name) {
    auto document = FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(document.has_value());
    document->program.name = name;
    document->program.fermentationStages.front().targetTemperatureCelsius =
        38.0;
    document->program.fermentationStages.front().durationMinutes = 120U;
    document->program.productSensorFailure.fallbackDelaySeconds = 30U;
    document->program.targetQualification.bandCelsius = 0.5;
    document->program.targetQualification.durationMinutes = 10U;
    document->program.maximumTargetReachMinutes = 180U;
    document->program.completion.mode = CompletionMode::CoolAndHoldForDuration;
    document->program.completion.coolingTargetCelsius = 9.0;
    document->program.completion.holdDurationMinutes = 30U;
    document->program.preheat = false;
    document->program.maximumProductWaitMinutes = std::nullopt;
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    const auto run =
        ActiveRun::start(*document, ProgramSourceKind::FactoryCatalog,
                         RunProgramSourceRevision{1U});
    TEST_ASSERT_TRUE(run.has_value());
    auto& state = FermentationApplicationTestAccess::runtimeState(application);
    state.processState.state = ProcessState::Fermenting;
    state.activeProgramRun = *run;
    state.activeRunId = std::string(48U, 'r');
    state.activeRunSensorMode = RunSensorMode::Product;
    state.processRunSnapshot = makeProcessRunSnapshot(*state.activeProgramRun);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
}

// Whole GET /api/v1/status path through the composed dispatcher (session
// check, application snapshot with the UI refresh tracker, encoding, response
// takeover), measured over operator new and cJSON. The encoder-only window is
// in test_run_detail_projection. Host measurement, not an ESP32 heap proof.
void test_language_selector_is_in_every_shell_state_without_server_state() {
    const std::string selector =
        "<select id=lang aria-label=Language><option value=de>Deutsch<option "
        "value=en>English<option value=es>Espa\xC3\xB1ol</select>";
    const auto check = [&](const std::string& body) {
        TEST_ASSERT_EQUAL_UINT32(1U, occurrences(body, "<select id=lang"));
        TEST_ASSERT_TRUE(contains(body, selector.c_str()));
        TEST_ASSERT_TRUE(body.find(selector) < body.find("<h1>"));
        // Exactly the three codes, validated and stored only in the browser.
        TEST_ASSERT_TRUE(contains(body, "ok=/^(de|en|es)$/"));
        TEST_ASSERT_TRUE(contains(body, "localStorage.lang"));
        TEST_ASSERT_FALSE(contains(body, "sessionStorage"));
        TEST_ASSERT_FALSE(contains(body, "document.cookie"));
        TEST_ASSERT_FALSE(contains(body, "navigator.languages"));
    };
    {
        ComposedFixture fixture;  // setup shell (unprovisioned)
        check(shellBody(fixture));
    }
    {
        ComposedFixture fixture;  // protected: anonymous and authenticated
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        check(shellBody(fixture));
        const auto session = loginComposed(fixture);
        check(authenticatedShell(fixture, session.cookie));
        // No server-side language state: no language cookie, no extra header.
        auto request = makeAuthenticatedRequest("GET", "/", session.cookie);
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    }
    {
        ComposedFixture fixture;  // password protection disabled
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::setWebPasswordEnabled(
                fixture.application, false));
        auto request = makeWebRequest("GET", "/");
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        check(shellDocument(response));
        TEST_ASSERT_TRUE(response.metadata.setCookie.has_value());
        TEST_ASSERT_FALSE(response.metadata.setCookie->find("lang") !=
                          std::string::npos);
    }
}

void test_language_choice_never_reaches_the_api_or_the_polling_contract() {
    ComposedFixture fixture;
    const auto body = loggedInShell(fixture);
    // The switch re-renders locally: same three polling paths, same single
    // interval and the unchanged mutation headers; no request in sl().
    TEST_ASSERT_EQUAL_UINT32(5U, occurrences(body, "fetch('"));
    TEST_ASSERT_EQUAL_UINT32(1U, occurrences(body, "setInterval("));
    TEST_ASSERT_TRUE(contains(body, "X-UI-Mutation-Seq"));
    TEST_ASSERT_TRUE(contains(body, "sw.onchange=()=>{sl(sw.value);ui();"));
    TEST_ASSERT_TRUE(contains(body, "try{localStorage.lang=lc}catch(e){}"));
    TEST_ASSERT_TRUE(contains(body, "b[X]=K===k?T.sure:T[k]+"));
    // The labels of the selector are not translated and carry no table key.
    TEST_ASSERT_FALSE(contains(body, "language:["));
}

void test_shell_device_info_is_bound_to_the_status_codec() {
    ComposedFixture fixture;
    const auto body = loggedInShell(fixture);
    FermentationUiSnapshot snapshot;
    FermentationDeviceInfoView device;
    device.deviceName = "Fermenter";
    device.timeZoneId = "Europe/Zurich";
    device_platform::LocalTime local;
    local.year = 2026;
    local.month = 10;
    local.day = 20;
    local.hour = 14;
    local.minute = 3;
    device.localTime = local;
    std::string status;
    TEST_ASSERT_TRUE(encodeWebApiStatus(snapshot, device, status));
    // Every member the codec writes is read by the shell.
    for (const char* member : {"deviceName", "networkState", "networkMode",
                               "clock", "local", "zone"}) {
        TEST_ASSERT_TRUE(
            contains(status, (std::string("\"") + member + "\":").c_str()));
        TEST_ASSERT_TRUE(contains(body, member));
    }
    // Every token the encoders can write has a label in all three locales; the
    // network words are their own keys, never the poll words online/offline.
    std::set<std::string> states;
    std::set<std::string> modes;
    for (unsigned value = 0U; value < 16U; ++value) {
        FermentationDeviceInfoView probe;
        probe.connection =
            static_cast<device_platform::DeviceUiNetworkStatus>(value);
        FermentationUiSnapshot plain;
        plain.network.selectionRequired = false;
        plain.network.currentMode =
            static_cast<device_platform::NetworkMode>(value);
        std::string encoded;
        if (encodeWebApiStatus(plain, probe, encoded)) {
            for (const auto& token : jsonTokens(encoded, "networkState")) {
                states.insert(token);
            }
            for (const auto& token : jsonTokens(encoded, "networkMode")) {
                modes.insert(token);
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(3U, static_cast<std::uint32_t>(states.size()));
    TEST_ASSERT_TRUE(states.count("connected") == 1U);
    TEST_ASSERT_TRUE(states.count("disconnected") == 1U);
    TEST_ASSERT_TRUE(states.count("unavailable") == 1U);
    TEST_ASSERT_TRUE(modes.count("ap-only") == 1U);
    TEST_ASSERT_TRUE(modes.count("home-wifi") == 1U);
    for (const char* key :
         {"network", "clock", "net-connected", "net-disconnected",
          "clock-untrusted", "ap-only", "home-wifi"}) {
        TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, key));
    }
    TEST_ASSERT_TRUE(contains(body, "T['net-'+a.networkState]"));
    // The shell reads only the fetched status; no browser clock is used for
    // the device time.
    TEST_ASSERT_FALSE(contains(body, "new Date("));
    TEST_ASSERT_FALSE(contains(body, "toLocale"));
}

void test_device_info_view_reports_name_zone_local_time_and_connection() {
    ComposedFixture fixture(true);
    auto info = fixture.application.deviceInfoView();
    TEST_ASSERT_TRUE(info.deviceName.has_value());
    TEST_ASSERT_FALSE(info.deviceName->empty());
    TEST_ASSERT_EQUAL_STRING("Europe/Zurich", info.timeZoneId.c_str());
    // No trusted UTC: no substitute time.
    TEST_ASSERT_FALSE(info.localTime.has_value());
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DeviceUiNetworkStatus::Connected),
        static_cast<int>(info.connection));

    // 2026-10-20 12:03:00 UTC is 14:03 CEST (EU rule, 7 days before the
    // switch back).
    fixture.timeSource.setUnixTimeSeconds(1792497780);
    info = fixture.application.deviceInfoView();
    TEST_ASSERT_TRUE(info.localTime.has_value());
    TEST_ASSERT_EQUAL_INT(2026, info.localTime->year);
    TEST_ASSERT_EQUAL_UINT8(10U, info.localTime->month);
    TEST_ASSERT_EQUAL_UINT8(20U, info.localTime->day);
    TEST_ASSERT_EQUAL_UINT8(14U, info.localTime->hour);
    TEST_ASSERT_EQUAL_UINT8(3U, info.localTime->minute);
    fixture.timeSource.setUnixTimeSeconds(std::nullopt);
    TEST_ASSERT_FALSE(
        fixture.application.deviceInfoView().localTime.has_value());
}

void test_device_info_connection_requires_the_configured_network_mode() {
    ComposedFixture fixture(true);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(NetworkConfigurationStatus::Applied),
        static_cast<int>(
            fixture.application
                .applyNetworkMode(device_platform::NetworkMode::HOME_WIFI)
                .status));
    using device_platform::DeviceUiNetworkStatus;
    const auto connection = [&] {
        return fixture.application.deviceInfoView().connection;
    };
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceUiNetworkStatus::Connected),
                      static_cast<int>(connection()));
    // The lifecycle runs another mode than the configured one: its state
    // must not be shown as the state of the chosen mode.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkOperationStatus::Applied),
        static_cast<int>(
            fixture.network.start(device_platform::NetworkMode::AP_ONLY, {})
                .status));
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceUiNetworkStatus::Unavailable),
                      static_cast<int>(connection()));
    // Same mode, transition in progress: Disconnected.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkOperationStatus::Applied),
        static_cast<int>(
            fixture.network.start(device_platform::NetworkMode::HOME_WIFI, {})
                .status));
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceUiNetworkStatus::Disconnected),
                      static_cast<int>(connection()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkOperationStatus::Applied),
        static_cast<int>(fixture.network.stop().status));
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceUiNetworkStatus::Unavailable),
                      static_cast<int>(connection()));
}

void test_status_endpoint_adds_device_info_without_network_secrets() {
    ComposedFixture fixture(true);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto session = loginComposed(fixture);
    const auto status = [&] {
        auto request =
            makeAuthenticatedRequest("GET", "/api/v1/status", session.cookie);
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
        return completeBody(response);
    };
    auto body = status();
    TEST_ASSERT_TRUE(contains(body, "\"deviceName\":\""));
    TEST_ASSERT_TRUE(contains(body, "\"networkState\":\"connected\""));
    TEST_ASSERT_TRUE(contains(body, "\"clock\":null"));
    TEST_ASSERT_TRUE(contains(body, "\"networkMode\":\"ap-only\""));
    fixture.timeSource.setUnixTimeSeconds(1792497780);
    body = status();
    TEST_ASSERT_TRUE(contains(body,
                              "\"clock\":{\"local\":\"2026-10-20 14:03\","
                              "\"zone\":\"Europe/Zurich\"}"));
    // No SoftAP password or address in the status. The SoftAP SSID is derived
    // from the device name, which is displayed on purpose, so it is not a
    // separate secret here.
    TEST_ASSERT_FALSE(
        contains(body, fixture.network.accessPointPassword().c_str()));
    TEST_ASSERT_FALSE(contains(body, "192.168"));
    TEST_ASSERT_TRUE(body.size() <= kMaximumWebApiResponseBodyBytes);
}

void test_status_codec_device_info_tokens_and_unavailable_parts() {
    FermentationUiSnapshot snapshot;
    const auto encode = [&](const FermentationDeviceInfoView& view) {
        std::string out;
        TEST_ASSERT_TRUE(encodeWebApiStatus(snapshot, view, out));
        TEST_ASSERT_TRUE(out.size() <= kMaximumWebApiResponseBodyBytes);
        return out;
    };
    FermentationDeviceInfoView view;
    auto body = encode(view);
    TEST_ASSERT_TRUE(contains(body, "\"deviceName\":null"));
    TEST_ASSERT_TRUE(contains(body, "\"networkState\":\"unavailable\""));
    TEST_ASSERT_TRUE(contains(body, "\"clock\":null"));
    using device_platform::DeviceUiNetworkStatus;
    view.connection = DeviceUiNetworkStatus::Connected;
    TEST_ASSERT_TRUE(contains(encode(view), "\"networkState\":\"connected\""));
    view.connection = DeviceUiNetworkStatus::Disconnected;
    TEST_ASSERT_TRUE(
        contains(encode(view), "\"networkState\":\"disconnected\""));
    // A time without a zone, or outside the four digit year range, is not
    // shown; the response itself stays a success.
    device_platform::LocalTime local;
    local.year = 2026;
    local.month = 10;
    local.day = 20;
    local.hour = 9;
    local.minute = 5;
    view.localTime = local;
    TEST_ASSERT_TRUE(contains(encode(view), "\"clock\":null"));
    view.timeZoneId = "Europe/Zurich";
    TEST_ASSERT_TRUE(contains(encode(view),
                              "\"clock\":{\"local\":\"2026-10-20 09:05\","
                              "\"zone\":\"Europe/Zurich\"}"));
    view.localTime->year = 10000;
    TEST_ASSERT_TRUE(contains(encode(view), "\"clock\":null"));
    view.localTime->year = -1;
    TEST_ASSERT_TRUE(contains(encode(view), "\"clock\":null"));
    // The 96 byte maximum name with a 64 byte zone id stays well inside the
    // bounded body.
    view.deviceName = std::string(96U, 'n');
    view.timeZoneId = std::string(64U, 'z');
    view.localTime->year = 2026;
    body = encode(view);
    TEST_ASSERT_TRUE(contains(body, std::string(96U, 'n').c_str()));
    TEST_ASSERT_TRUE(contains(body, std::string(64U, 'z').c_str()));
}

void test_status_handler_allocation_window_covers_snapshot_encode_and_response() {
    ComposedFixture fixture(true);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto session = loginComposed(fixture);
    auto* routes = fixture.http.routes();

    const auto handle = [&](bool expectRun, const char* label) {
        auto request =
            makeAuthenticatedRequest("GET", "/api/v1/status", session.cookie);
        device_platform::HttpResponse response;
        bool handled = false;
        const auto window = alloc_window::measure(
            [&] { handled = routes->handle(request, response); });
        TEST_ASSERT_TRUE(handled);
        TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
        const auto body = completeBody(response);
        TEST_ASSERT_TRUE(contains(body, "\"version\":1,\"ready\":"));
        TEST_ASSERT_TRUE(contains(body, "\"revisions\":{"));
        TEST_ASSERT_TRUE(body.size() <= kMaximumWebApiResponseBodyBytes);
        if (expectRun) {
            TEST_ASSERT_TRUE(contains(body, "\"run\":{\"id\":\"rrrrrrrr"));
            TEST_ASSERT_TRUE(contains(body, "\"programName\":\"\xC3\xA4"));
            TEST_ASSERT_TRUE(contains(body, "\"targetCelsius\":38"));
            TEST_ASSERT_TRUE(contains(body, "\"mode\":\"cool-hold-duration\""));
            TEST_ASSERT_TRUE(contains(body, "\"scope\":\"phase\""));
        } else {
            TEST_ASSERT_TRUE(contains(body, "\"run\":null"));
        }
        TEST_ASSERT_EQUAL_UINT32(
            0U, static_cast<std::uint32_t>(window.cJsonLiveBytes));
        TEST_ASSERT_TRUE(window.count > 0U);
        TEST_ASSERT_TRUE(window.peakLiveBytes >= body.size());
        std::printf(
            "STATUS_HANDLER_ALLOC %s: count=%zu bytes=%zu largest=%zu "
            "peak_live=%zu cjson_count=%zu body=%zu\n",
            label, window.count, window.requestedBytes, window.largestBlock,
            window.peakLiveBytes, window.cJsonCount, body.size());
        return window;
    };

    // Standby: the first call also fills the refresh-tracker copy.
    handle(false, "standby cold");
    const auto standbyWarm = handle(false, "standby warm");

    // 48 two byte scalars: the 96 byte maximum of a visible name.
    std::string name;
    for (int i = 0; i < 48; ++i) name += "\xC3\xA4";
    installActiveRunForStatus(fixture.application, name);
    const auto runCold = handle(true, "active run, 96 B name, cold");
    const auto runWarm = handle(true, "active run, 96 B name, warm");
    // The run block adds allocations over standby; a repeated request with an
    // unchanged state never needs more than the cold one.
    TEST_ASSERT_TRUE(runWarm.requestedBytes > standbyWarm.requestedBytes);
    TEST_ASSERT_TRUE(runWarm.requestedBytes <= runCold.requestedBytes);
}

void test_dashboard_shell_labels_cover_every_codec_token() {
    ComposedFixture fixture;
    const auto body = loggedInShell(fixture);
    // SIM-27-W-03: the name tables are private to the codec, so the tokens are
    // collected through the public encoders over the whole enum range; a new
    // enumerator changes the count or lacks a label and fails here.
    std::set<std::string> modes;
    std::set<std::string> roles;
    std::set<std::string> qualities;
    for (unsigned value = 0U; value < 16U; ++value) {
        FermentationUiSnapshot snapshot;
        snapshot.home.mode = static_cast<FermentationHomeMode>(value);
        std::string status;
        if (encodeWebApiStatus(snapshot, FermentationDeviceInfoView{},
                               status)) {
            for (const auto& token : jsonTokens(status, "homeMode")) {
                modes.insert(token);
            }
        }
        FermentationUiSnapshot temperatures;
        temperatures.temperatures = {
            {static_cast<FermentationTemperatureRole>(value), 1.0, {}}};
        temperatures.temperatures[0].quality.quality =
            static_cast<device_platform::SensorQuality>(value);
        std::string encoded;
        if (encodeWebApiTemperatures(temperatures, encoded)) {
            for (const auto& token : jsonTokens(encoded, "role")) {
                roles.insert(token);
            }
            for (const auto& token : jsonTokens(encoded, "quality")) {
                qualities.insert(token);
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(7U, static_cast<std::uint32_t>(modes.size()));
    TEST_ASSERT_EQUAL_UINT32(3U, static_cast<std::uint32_t>(roles.size()));
    TEST_ASSERT_EQUAL_UINT32(3U, static_cast<std::uint32_t>(qualities.size()));
    for (const auto& token : modes) {
        TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, token));
    }
    for (const auto& token : roles) {
        TEST_ASSERT_EQUAL_UINT32(3U, labelCount(body, token));
    }
    // `valid` needs no reason text; `stale` and `failed` do.
    for (const auto& token : qualities) {
        TEST_ASSERT_EQUAL_UINT32(token == "valid" ? 0U : 3U,
                                 labelCount(body, token));
    }
    // Severity classes that carry a colour exist in the shell style.
    TEST_ASSERT_TRUE(contains(body, ".safety-fault"));
    TEST_ASSERT_TRUE(contains(body, ".decision-required"));
}

void test_dashboard_feed_fields_in_standby_and_constructed_states() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, passwordUnderTest(), kPinUnderTest));
    const auto session = loginComposed(fixture);
    // SIM-27-W-04, standby through the real Application path.
    device_platform::HttpResponse response;
    auto request =
        makeAuthenticatedRequest("GET", "/api/v1/status", session.cookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(
        contains(completeBody(response), "\"homeMode\":\"standby\""));
    TEST_ASSERT_TRUE(contains(completeBody(response), "\"revisions\":{"));
    TEST_ASSERT_TRUE(contains(completeBody(response), "\"programCatalog\":\""));
    request =
        makeAuthenticatedRequest("GET", "/api/v1/temperatures", session.cookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_TRUE(contains(completeBody(response), "\"temperatures\":["));
    request = makeAuthenticatedRequest("GET", "/api/v1/alerts", session.cookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_TRUE(contains(completeBody(response), "\"alerts\":["));

    // The other modes are not reachable through the real path in this fixture
    // (no program is startable without commissioning values, see commit
    // 0b7cea0): field contract over constructed snapshots only.
    for (const auto mode :
         {FermentationHomeMode::Standby, FermentationHomeMode::ActiveRun,
          FermentationHomeMode::Waiting, FermentationHomeMode::Completed,
          FermentationHomeMode::Restricted, FermentationHomeMode::Recovery,
          FermentationHomeMode::Unavailable}) {
        FermentationUiSnapshot snapshot;
        snapshot.home.mode = mode;
        snapshot.revisions.expectedRunRevision = 7U;
        snapshot.revisions.expectedProgramCatalogRevision =
            ProgramCatalogRevision{3U};
        snapshot.temperatures = {
            {FermentationTemperatureRole::CabinetAir, 21.5, {}},
            {FermentationTemperatureRole::Product, 18.0, {}},
            {FermentationTemperatureRole::Cooling, std::nullopt, {}}};
        snapshot.temperatures[0].quality.quality =
            device_platform::SensorQuality::Valid;
        RuntimeMessage message;
        message.id = 5U;
        message.code = MessageCode::ProductInsertionRequested;
        message.messageClass = MessageClass::SafetyFault;
        message.active = true;
        snapshot.messages.push_back(MessageView{message});
        std::string status;
        std::string temperatures;
        std::string alerts;
        TEST_ASSERT_TRUE(
            encodeWebApiStatus(snapshot, FermentationDeviceInfoView{}, status));
        TEST_ASSERT_TRUE(encodeWebApiTemperatures(snapshot, temperatures));
        TEST_ASSERT_TRUE(encodeWebApiAlerts(snapshot, alerts));
        for (const char* field :
             {"\"homeMode\":\"", "\"run\":7", "\"programCatalog\":\"3\""}) {
            TEST_ASSERT_TRUE(contains(status, field));
        }
        for (const char* field : {"\"role\":\"", "\"quality\":\"",
                                  "\"valid\":", "\"valueCelsius\":"}) {
            TEST_ASSERT_TRUE(contains(temperatures, field));
        }
        for (const char* field :
             {"\"id\":5", "\"code\":\"", "\"severity\":\"safety-fault\"",
              "\"active\":true", "\"acknowledged\":", "\"muted\":"}) {
            TEST_ASSERT_TRUE(contains(alerts, field));
        }
    }
}

void test_dashboard_missing_and_stale_data_contract() {
    // SIM-27-W-05, codec side: an unavailable reading never carries a value;
    // a missing role or an empty list is simply absent.
    FermentationUiSnapshot snapshot;
    std::string temperatures;
    TEST_ASSERT_TRUE(encodeWebApiTemperatures(snapshot, temperatures));
    TEST_ASSERT_TRUE(contains(temperatures, "\"temperatures\":[]"));
    snapshot.temperatures = {{FermentationTemperatureRole::Product, 19.0, {}}};
    snapshot.temperatures[0].quality.quality =
        device_platform::SensorQuality::Stale;
    TEST_ASSERT_TRUE(encodeWebApiTemperatures(snapshot, temperatures));
    TEST_ASSERT_TRUE(contains(temperatures, "\"role\":\"product\""));
    TEST_ASSERT_FALSE(contains(temperatures, "cabinet-air"));
    TEST_ASSERT_TRUE(contains(
        temperatures,
        "\"quality\":\"stale\",\"valid\":false,\"valueCelsius\":null"));
    TEST_ASSERT_FALSE(contains(temperatures, "19"));
}

void test_dashboard_ordering_guard_and_age_contract_in_the_shell_text() {
    ComposedFixture fixture;
    const auto body = loggedInShell(fixture);
    const auto rf = refreshFunction(body);
    // SIM-27-W-05, structure: generation and start time before the first fetch.
    const auto generation = rf.find("my=++G");
    TEST_ASSERT_TRUE(contains(body, "N=Date.now"));
    const auto started = rf.find("t0=N()");
    const auto firstFetch = rf.find("fetch(");
    TEST_ASSERT_TRUE(generation != std::string::npos);
    TEST_ASSERT_TRUE(started != std::string::npos);
    TEST_ASSERT_TRUE(firstFetch != std::string::npos);
    TEST_ASSERT_TRUE(generation < firstFetch);
    TEST_ASSERT_TRUE(started < firstFetch);
    // `A` stays the authentication state; G also invalidates pre-mutation
    // groups.
    TEST_ASSERT_TRUE(contains(body, "G=0,F=0,C=0,W=0"));
    TEST_ASSERT_FALSE(contains(body, "my<A"));
    TEST_ASSERT_FALSE(contains(body, "A=my"));
    // Success path: guard after the awaited responses, before any effect.
    const auto awaited = rf.find("await Promise.all(r)");
    const auto guard = rf.find("if(my!==G||c.signal.aborted)return;", awaited);
    const auto displayCopy = rf.find("LP=p", guard);
    const auto gating = rf.find("P=N()-t0<=15000", guard);
    TEST_ASSERT_TRUE(awaited != std::string::npos);
    TEST_ASSERT_TRUE(guard != std::string::npos);
    TEST_ASSERT_TRUE(displayCopy != std::string::npos);
    TEST_ASSERT_TRUE(gating != std::string::npos);
    TEST_ASSERT_TRUE(awaited < guard);
    // Age is the start of the request, never the arrival time.
    TEST_ASSERT_TRUE(contains(rf, "LT=t0"));
    TEST_ASSERT_FALSE(contains(rf, "LT=N()"));
    // Shared timeout/error callback: generation guard, P=null, action lock.
    const auto handler = rf.find("const e=()=>{c.abort();");
    const auto failGuard = rf.find("if(my===G){", handler);
    const auto failClear = rf.find("P=null;", handler);
    const auto failLock = rf.find("ui();", failClear);
    TEST_ASSERT_TRUE(handler != std::string::npos);
    TEST_ASSERT_TRUE(failGuard < failClear);
    TEST_ASSERT_TRUE(failClear < failLock);
    // Age threshold is evaluated at the start of the tick, before any fetch,
    // and depends only on LT (a hanging fetch cannot suppress it).
    const auto threshold = rf.find("t0-LT>15000");
    TEST_ASSERT_TRUE(threshold != std::string::npos);
    TEST_ASSERT_TRUE(threshold < firstFetch);
    TEST_ASSERT_TRUE(contains(rf, "P=null"));
    // P (gating) and LP (display copy) stay separate: ui() never reads LP.
    TEST_ASSERT_FALSE(contains(uiFunction(body), "LP"));
    TEST_ASSERT_TRUE(contains(body, "b.disabled=B||!P||d[k]"));
    // A value is never shown for an unavailable reading.
    TEST_ASSERT_TRUE(contains(body, "e.valid&&e.valueCelsius!=null?"));
}

// ---- C4: read-only program list ---------------------------------------------

device_platform::HttpResponse getPrograms(ComposedFixture& fixture,
                                          const std::string& cookie) {
    auto request = cookie.empty() ? makeWebRequest("GET", "/api/v1/programs")
                                  : makeAuthenticatedRequest(
                                        "GET", "/api/v1/programs", cookie);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    return response;
}

void test_programs_route_gate_and_request_contract() {
    ComposedFixture fixture;
    // Unprovisioned: no authentication yet.
    TEST_ASSERT_EQUAL_UINT16(503U, getPrograms(fixture, "").statusCode);
    auto session = composedRunSession(fixture);
    TEST_ASSERT_EQUAL_UINT16(401U, getPrograms(fixture, "").statusCode);
    TEST_ASSERT_EQUAL_UINT16(401U, getPrograms(fixture, "bogus").statusCode);

    auto request = makeAuthenticatedRequest("POST", "/api/v1/programs",
                                            session.web.cookie);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(405U, response.statusCode);
    request =
        makeAuthenticatedRequest("GET", "/api/v1/programs", session.web.cookie);
    request.body = "x";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);
}

void test_programs_route_lists_catalog_names_with_the_status_revision() {
    ComposedFixture fixture;
    auto session = composedRunSession(fixture);
    const auto response = getPrograms(fixture, session.web.cookie);
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(completeBody(response).size() <=
                     kMaximumWebApiResponseBodyBytes);
    const auto source = fixture.application.uiPresentationSource();
    TEST_ASSERT_TRUE(source.has_value());
    TEST_ASSERT_TRUE(!source->programCatalog.programs.empty());
    // The revision is the same decimal string the status exposes.
    const auto revision =
        std::to_string(fixture.application.uiSnapshot()
                           .revisions.expectedProgramCatalogRevision->value());
    TEST_ASSERT_TRUE(
        contains(completeBody(response),
                 ("\"catalogRevision\":\"" + revision + "\"").c_str()));
    // Only id and name per entry; no notes, stages or device data.
    for (const auto& entry : source->programCatalog.programs) {
        if (!entry.program.installed || !entry.program.enabled) continue;
        TEST_ASSERT_TRUE(
            contains(completeBody(response),
                     ("{\"id\":\"" + entry.program.id + "\",\"name\":\"" +
                      entry.program.name + "\"}")
                         .c_str()));
    }
    TEST_ASSERT_FALSE(contains(completeBody(response), "notes"));
    TEST_ASSERT_FALSE(contains(completeBody(response), "stages"));
    // Reading the list does not advance the mutation sequence.
    TEST_ASSERT_TRUE(
        contains(authenticatedShell(fixture, session.web.cookie), "let Q=1;"));
}

void test_programs_encoder_filters_empty_and_bounded() {
    FermentationUiPresentationSource source;
    source.programCatalogRevision = ProgramCatalogRevision{7U};
    std::string body;
    // A genuinely empty catalog is an empty list, not an error.
    TEST_ASSERT_TRUE(encodeWebApiPrograms(source, body));
    TEST_ASSERT_TRUE(contains(body, "\"catalogRevision\":\"7\""));
    TEST_ASSERT_TRUE(contains(body, "\"programs\":[]"));

    ProgramDocument visible;
    visible.program.id = "a";
    visible.program.name = "A";
    ProgramDocument hidden = visible;
    hidden.program.id = "b";
    hidden.program.enabled = false;
    ProgramDocument removed = visible;
    removed.program.id = "c";
    removed.program.installed = false;
    source.programCatalog.programs = {visible, hidden, removed};
    TEST_ASSERT_TRUE(encodeWebApiPrograms(source, body));
    TEST_ASSERT_TRUE(contains(body, "{\"id\":\"a\",\"name\":\"A\"}"));
    TEST_ASSERT_FALSE(contains(body, "\"b\""));
    TEST_ASSERT_FALSE(contains(body, "\"c\""));

    // The canonical count limit applies even when the body would fit.
    visible.program.name = "n";
    source.programCatalog.programs.assign(16U, visible);
    TEST_ASSERT_TRUE(encodeWebApiPrograms(source, body));
    TEST_ASSERT_TRUE(body.size() < kMaximumWebApiResponseBodyBytes);
    source.programCatalog.programs.assign(17U, visible);
    body.clear();
    TEST_ASSERT_FALSE(encodeWebApiPrograms(source, body));
    TEST_ASSERT_TRUE(body.empty());

    // Over the response budget: refused (the route answers 503), never cut.
    visible.program.name = std::string(400U, 'n');
    source.programCatalog.programs.assign(16U, visible);
    TEST_ASSERT_FALSE(encodeWebApiPrograms(source, body));
}

void test_programs_route_without_catalog_source_is_503_not_empty() {
    FermentationApplication application;  // never begun: no catalog source
    TEST_ASSERT_FALSE(application.uiPresentationSource().has_value());
    WebReadOnlyApiHandler api(application);
    auto request = makeWebRequest("GET", "/api/v1/programs");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    TEST_ASSERT_FALSE(contains(completeBody(response), "\"programs\""));
}

void test_shell_start_flow_text_contract() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, passwordUnderTest(), kPinUnderTest));
    const auto body = shellBody(fixture);
    // Names are shown, ids are sent; the list is only requested with a
    // usable sequence and its result never makes Start available by itself.
    TEST_ASSERT_TRUE(contains(body, "get('programs')"));
    TEST_ASSERT_TRUE(contains(body, "new Option(x.name,x.id)"));
    TEST_ASSERT_TRUE(contains(body, "{t:'start-program',c:sc()}"));
    TEST_ASSERT_TRUE(contains(body, "start:ps.disabled"));
    // Start needs two clicks like Stop/Complete (no direct-send branch).
    TEST_ASSERT_TRUE(contains(body, "if(a==='ack'||a==='mute'){"));
    TEST_ASSERT_FALSE(contains(body, "a==='start'&&"));
    // The start is bound to the catalog revision of the loaded list: it is
    // enabled only while list and status revision agree, a mismatch or failed
    // load clears the selection and reloads, and the intent carries the
    // list revision instead of the latest status revision.
    TEST_ASSERT_TRUE(contains(body, "!PC||!va()") &&
                     contains(body, "rv()[Z]!==PC"));
    TEST_ASSERT_TRUE(contains(body, "v&&v!==PC)lp();"));
    TEST_ASSERT_TRUE(contains(body, "PC='';pg[Y]();"));
    TEST_ASSERT_TRUE(contains(body, "PC=j.catalogRevision;"));
    TEST_ASSERT_TRUE(contains(body, "if(i.t==='start-program')o.c=PC;"));
    TEST_ASSERT_TRUE(contains(
        body,
        "rv()[Z],PC,pg.value,JSON.stringify(sc()),SD?.revision,sn.value]"));
}

void test_other_states_get_no_provisioning_form_or_bypass() {
    // Protected, disabled and recovery states keep the existing shell and
    // never offer a provisioning path.
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        const auto body = shellBody(fixture);
        TEST_ASSERT_FALSE(contains(body, "/api/v1/provision"));
        TEST_ASSERT_TRUE(contains(body, "/api/v1/login"));
    }
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
            fixture.application, passwordUnderTest(), kPinUnderTest));
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::setWebPasswordEnabled(
                fixture.application, false));
        auto request = makeWebRequest("GET", "/");
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_FALSE(
            contains(completeBody(response), "/api/v1/provision"));
    }
    {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(FermentationApplicationTestAccess::forceRootState(
            fixture.application, AuthProvisioningState::RecoveryRequired));
        const auto body = shellBody(fixture);
        TEST_ASSERT_FALSE(contains(body, "/api/v1/provision"));
        TEST_ASSERT_TRUE(
            contains(body, "Local provisioning or recovery required"));
    }
}

// Issue #19 (plan 4.4, invariant 1): the factory reset is local only. No HTTP
// route reaches the flow or the reset core; a probe of plausible paths and
// methods never succeeds and never arms the flow.
void test_no_http_route_reaches_the_local_factory_reset() {
    ComposedFixture fixture;
    fixture.application.setFactoryResetHoldMillis(1500U);
    auto* routes = fixture.http.routes();
    TEST_ASSERT_NOT_NULL(routes);
    const char* const paths[] = {
        "/api/v1/factory-reset",      "/api/v1/reset",      "/api/v1/reset-all",
        "/internal/ui/factory-reset", "/internal/ui/reset", "/factory-reset"};
    for (const char* path : paths) {
        for (const char* method : {"GET", "POST", "PUT", "DELETE"}) {
            auto request = makeWebRequest(method, path, "{}");
            request.metadata.contentType = "application/json";
            device_platform::HttpResponse response;
            const bool handled = routes->handle(request, response);
            TEST_ASSERT_TRUE(!handled || response.statusCode >= 400U);
        }
    }
    const auto view = fixture.application.factoryResetView(0U);
    TEST_ASSERT_TRUE(FactoryResetStage::Idle == view.stage);
    TEST_ASSERT_TRUE(FactoryResetOutcome::None == view.outcome);
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(fixture.http.stopCount()));
}

void test_provision_commit_outcome_unknown_closes_window_immediately() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    // The first bootstrap write (root -> Provisioning) is durably committed
    // but reports an unknown outcome, and the readback fails as well, so the
    // domain cannot resolve it (a successful readback would).
    fixture.store.setNextWriteFault(
        device_platform_test_support::SimulatedPersistentStateStore::
            WriteFault::PowerCutAfterCommitBeforeReturn);
    fixture.store.failNextReadAfterWrite();
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::RecoveryRequired);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() !=
                     WebAuthenticationState::Unprovisioned);
    TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() !=
                     WebAuthenticationState::PasswordProtected);
    TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
        fixture.application, 0U));
}

void test_provision_lost_race_closes_window_via_current_state() {
    // The loser of a provisioning race sees a provisioned root; its window
    // is closed by the current-state conclusion, not only by the winner.
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::Provisioned);
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::windowFlagOpen(fixture.application));
}

void test_provision_result_projection_is_fail_closed() {
    const auto project = FermentationApplicationTestAccess::projectProvision;
    using S = AuthBootstrapStatus;
    TEST_ASSERT_TRUE(project(S::BootstrapAllowed, S::RecoveryRequired) ==
                     WebProvisionStatus::Provisioned);
    TEST_ASSERT_TRUE(project(S::InvalidInput, S::BootstrapAllowed) ==
                     WebProvisionStatus::InvalidCredentials);
    // The concurrent-winner case: bootstrap saw a non-Unprovisioned root and
    // the re-inspection inside the auth token reports provisioned.
    TEST_ASSERT_TRUE(project(S::RecoveryRequired, S::AlreadyProvisioned) ==
                     WebProvisionStatus::AlreadyProvisioned);
    TEST_ASSERT_TRUE(project(S::RecoveryRequired, S::RecoveryRequired) ==
                     WebProvisionStatus::RecoveryRequired);
    TEST_ASSERT_TRUE(project(S::RecoveryRequired, S::BootstrapAllowed) ==
                     WebProvisionStatus::RecoveryRequired);
    TEST_ASSERT_TRUE(project(S::KdfUnavailable, S::RecoveryRequired) ==
                     WebProvisionStatus::Failed);
    TEST_ASSERT_TRUE(project(S::PersistenceFailure, S::RecoveryRequired) ==
                     WebProvisionStatus::Failed);
    TEST_ASSERT_TRUE(project(S::CommitOutcomeUnknown, S::RecoveryRequired) ==
                     WebProvisionStatus::RecoveryRequired);
    TEST_ASSERT_TRUE(project(S::LockedOut, S::RecoveryRequired) ==
                     WebProvisionStatus::RecoveryRequired);
}

void test_provision_gate_closed_is_fail_closed_without_kdf() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    FermentationApplicationTestAccess::closeAuthGate(fixture.application);
    const auto calls = fixture.kdf.calls();
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::RecoveryRequired);
    TEST_ASSERT_EQUAL_UINT(calls, fixture.kdf.calls());
    FermentationApplicationTestAccess::reopenAuthGate(fixture.application);
    TEST_ASSERT_TRUE(provisionProtected(fixture) ==
                     WebProvisionStatus::Provisioned);
}

void test_provision_running_during_factory_reset_is_not_reported_current() {
    for (int iteration = 0; iteration < 4; ++iteration) {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        const auto recordsBefore = authRecordBytes(fixture);
        fixture.kdf.arm();
        WebProvisionStatus provisioned = WebProvisionStatus::Provisioned;
        std::thread provisioner(
            [&] { provisioned = provisionProtected(fixture); });
        TEST_ASSERT_TRUE(fixture.kdf.waitEntered());
        std::atomic<bool> resetDone{false};
        ConfigurationRecoveryResult reset;
        std::thread resetter([&] {
            reset = fixture.application.beginAuthorizedFactoryReset();
            resetDone = true;
        });
        TEST_ASSERT_TRUE(eventually([&] {
            return FermentationApplicationTestAccess::authGateClosed(
                fixture.application);
        }));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // The reset waits for the running provisioning before it touches
        // the store.
        TEST_ASSERT_FALSE(resetDone.load());
        TEST_ASSERT_TRUE(authRecordBytes(fixture) == recordsBefore);
        fixture.kdf.release();
        provisioner.join();
        resetter.join();
        TEST_ASSERT_TRUE(resetFinished(reset));
        // The bootstrap ran against the old epoch; the reset crossed the
        // trust boundary before the result was mapped: not current success.
        TEST_ASSERT_TRUE(provisioned == WebProvisionStatus::RecoveryRequired);
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::windowFlagOpen(
            fixture.application));
        TEST_ASSERT_FALSE(FermentationApplicationTestAccess::hasActiveSession(
            fixture.application, 0U));
        if (reset.status ==
            ConfigurationRecoveryStatus::FactoryResetCompleted) {
            TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                             WebAuthenticationState::Unprovisioned);
        }
    }
}

void test_concurrent_provisioning_has_one_winner_and_one_credential_write() {
    for (int iteration = 0; iteration < 12; ++iteration) {
        ComposedFixture fixture;
        TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
        std::atomic<bool> go{false};
        WebProvisionStatus first = WebProvisionStatus::Failed;
        WebProvisionStatus second = WebProvisionStatus::Failed;
        auto worker = [&](WebProvisionStatus& out) {
            while (!go.load()) {
                std::this_thread::yield();
            }
            out = provisionProtected(fixture);
        };
        std::thread a(worker, std::ref(first));
        std::thread b(worker, std::ref(second));
        go = true;
        a.join();
        b.join();
        const bool firstWon = first == WebProvisionStatus::Provisioned;
        const bool secondWon = second == WebProvisionStatus::Provisioned;
        TEST_ASSERT_TRUE(firstWon != secondWon);
        TEST_ASSERT_TRUE((firstWon ? second : first) ==
                         WebProvisionStatus::AlreadyProvisioned);
        // One bootstrap: password + Service-PIN derivation, exactly once.
        TEST_ASSERT_EQUAL_UINT(2U, fixture.kdf.calls());
        TEST_ASSERT_TRUE(fixture.application.webAuthenticationState() ==
                         WebAuthenticationState::PasswordProtected);
    }
}

void test_destructor_drains_a_running_login() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    DeterministicRandom random;
    BlockableKdf kdf;
    CapturingHttpServerLifecycle http;
    TEST_ASSERT_TRUE(platform.begin({true}));
    auto application = std::make_unique<FermentationApplication>();
    TEST_ASSERT_TRUE(application->begin(platform, store, timeZoneResolver,
                                        timeSource, network, http, random,
                                        kdf));
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        *application, "correct horse battery", "1234"));

    kdf.arm();
    WebAuthenticationResult loginResult;
    std::thread login([&] {
        loginResult = application->authenticateWebPassword(
            "correct horse battery", 1000U);
    });
    TEST_ASSERT_TRUE(kdf.waitEntered());
    std::atomic<bool> destroyed{false};
    std::thread destroyer([&] {
        application.reset();
        destroyed = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT_FALSE(destroyed.load());
    kdf.release();
    login.join();
    destroyer.join();
    TEST_ASSERT_TRUE(destroyed.load());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WebAuthenticationResultStatus::Authenticated),
        static_cast<int>(loginResult.status));
}

void test_auth_operation_gate_object_size_is_reported() {
    // S2 evidence: static object size of the gate (heap of the pthread
    // objects is measured on the target, not here).
    std::printf("AUTH_OPERATION_GATE_SIZEOF=%u\n",
                static_cast<unsigned>(sizeof(AuthOperationGate)));
    TEST_ASSERT_TRUE(sizeof(AuthOperationGate) > 0U);
}

// ---- Issue #188 A: local Service-PIN check vs. reset and re-init (6.1a) ---

void assertLocalPinStatus(LocalServicePinStatus expected,
                          const LocalServicePinResult& result) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected),
                          static_cast<int>(result.status));
}

void test_local_pin_check_and_factory_reset_neither_hang_nor_grant_access() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    fixture.kdf.arm();
    LocalServicePinResult pinResult;
    std::thread check(
        [&] { pinResult = fixture.application.verifyLocalServicePin("1234"); });
    TEST_ASSERT_TRUE(fixture.kdf.waitEntered());

    std::atomic<bool> resetDone{false};
    ConfigurationRecoveryResult reset;
    std::thread resetter([&] {
        reset = fixture.application.beginAuthorizedFactoryReset();
        resetDone = true;
    });
    // The reset closed the gate and waits for the running PIN check.
    TEST_ASSERT_TRUE(eventually([&] {
        return FermentationApplicationTestAccess::authGateClosed(
            fixture.application);
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT_FALSE(resetDone.load());

    fixture.kdf.release();
    check.join();
    resetter.join();
    TEST_ASSERT_TRUE(resetFinished(reset));
    // The check finished against the old context: no access afterwards.
    assertLocalPinStatus(LocalServicePinStatus::Unavailable, pinResult);
    TEST_ASSERT_FALSE(fixture.application.uiSnapshot().service.available);
}

void test_local_pin_check_after_a_failed_reset_still_authorizes() {
    // A failed reset reopens the gate and keeps the domain; an in-flight
    // variant needs a counting write fault the store does not offer (every
    // PIN check writes), so the sequence is checked here.
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    fixture.store.setNextWriteFault(
        device_platform_test_support::SimulatedPersistentStateStore::
            WriteFault::FailBeforeBegin);
    const auto reset = fixture.application.beginAuthorizedFactoryReset();
    TEST_ASSERT_TRUE(reset.status !=
                     ConfigurationRecoveryStatus::FactoryResetCompleted);
    assertLocalPinStatus(LocalServicePinStatus::Authorized,
                         fixture.application.verifyLocalServicePin("1234"));
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().service.available);
}

void test_local_pin_check_after_gate_close_is_unavailable_without_kdf() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    FermentationApplicationTestAccess::closeAuthGate(fixture.application);
    const auto callsBefore = fixture.kdf.calls();
    assertLocalPinStatus(LocalServicePinStatus::Unavailable,
                         fixture.application.verifyLocalServicePin("1234"));
    TEST_ASSERT_EQUAL_UINT(callsBefore, fixture.kdf.calls());
    FermentationApplicationTestAccess::reopenAuthGate(fixture.application);
    assertLocalPinStatus(LocalServicePinStatus::Authorized,
                         fixture.application.verifyLocalServicePin("1234"));
}

void test_local_pin_check_refuses_access_after_a_same_epoch_reinit() {
    ComposedFixture fixture;
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    const auto identityBefore =
        FermentationApplicationTestAccess::authContextIdentity(
            fixture.application);
    const auto generationBefore =
        FermentationApplicationTestAccess::trustGeneration(fixture.application);
    fixture.kdf.arm();
    LocalServicePinResult pinResult;
    std::thread check(
        [&] { pinResult = fixture.application.verifyLocalServicePin("1234"); });
    TEST_ASSERT_TRUE(fixture.kdf.waitEntered());
    std::atomic<bool> reinitDone{false};
    std::thread reinit([&] {
        FermentationApplicationTestAccess::reinitializeAuthentication(
            fixture.application);
        reinitDone = true;
    });
    TEST_ASSERT_TRUE(eventually([&] {
        return FermentationApplicationTestAccess::authGateClosed(
            fixture.application);
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TEST_ASSERT_FALSE(reinitDone.load());
    fixture.kdf.release();
    check.join();
    reinit.join();

    // Same epoch and bootstrap sequence, but a new domain and generation.
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::authContextIdentity(
                         fixture.application) == identityBefore);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::trustGeneration(
                         fixture.application) > generationBefore);
    assertLocalPinStatus(LocalServicePinStatus::Unavailable, pinResult);
    TEST_ASSERT_FALSE(fixture.application.uiSnapshot().service.available);
}

void test_existing_local_lease_survives_a_network_trust_boundary() {
    ComposedFixture fixture(true);
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
        fixture.application, "correct horse battery", "1234"));
    assertLocalPinStatus(LocalServicePinStatus::Authorized,
                         fixture.application.verifyLocalServicePin("1234"));
    const auto generationBefore =
        FermentationApplicationTestAccess::trustGeneration(fixture.application);
    static_cast<void>(fixture.application.applyNetworkMode(
        device_platform::NetworkMode::HOME_WIFI));
    TEST_ASSERT_TRUE(FermentationApplicationTestAccess::trustGeneration(
                         fixture.application) > generationBefore);
    // Browser sessions are revoked; the local lease is not network-bound.
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().service.available);
}

void test_split_shell_matches_frozen_generator_and_clears_suffix_on_errors() {
    for (const auto state : {WebAuthenticationState::Unprovisioned,
                             WebAuthenticationState::PasswordProtected,
                             WebAuthenticationState::PasswordDisabled,
                             WebAuthenticationState::RecoveryRequired}) {
        ComposedFixture fixture;
        if (state != WebAuthenticationState::Unprovisioned) {
            TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
                fixture.application, passwordUnderTest(), kPinUnderTest));
        }
        if (state == WebAuthenticationState::PasswordDisabled) {
            TEST_ASSERT_TRUE(
                FermentationApplicationTestAccess::setWebPasswordEnabled(
                    fixture.application, false));
        }
        if (state == WebAuthenticationState::RecoveryRequired) {
            TEST_ASSERT_TRUE(FermentationApplicationTestAccess::forceRootState(
                fixture.application, AuthProvisioningState::RecoveryRequired));
        }
        device_platform::HttpResponse response;
        auto request = makeWebRequest("GET", "/");
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_FALSE(response.staticBodySuffix.empty());
        // The dynamic bootstrap is already present in the owned head. Extract
        // it rather than assert only fragments of the constant suffix.
        const auto head = response.body;
        auto csrfStart = head.find("const S='");
        std::string csrf;
        bool authenticated = false;
        std::uint64_t sequence = 0U;
        if (csrfStart != std::string::npos) {
            csrfStart += 9U;
            csrf =
                head.substr(csrfStart, head.find("';", csrfStart) - csrfStart);
            authenticated = head.find("const U=true") != std::string::npos;
            const auto seqStart = head.find("let Q=") + 6U;
            sequence = std::stoull(head.substr(seqStart));
        }
        const auto expected =
            shell_golden::webShell(state, csrf, authenticated, sequence);
        if (state == WebAuthenticationState::Unprovisioned) {
            TEST_ASSERT_EQUAL_STRING(expected.c_str(),
                                     shellDocument(response).c_str());
        } else {
            const auto split = expected.find(";const E=");
            TEST_ASSERT_EQUAL_STRING(
                expected.substr(0U, split).c_str(),
                shellDocument(response).substr(0U, split).c_str());
        }
        if (state != WebAuthenticationState::Unprovisioned) {
            TEST_ASSERT_LESS_OR_EQUAL_UINT32(1920U,
                                             completeBody(response).size());
        }
        if (state == WebAuthenticationState::PasswordProtected) {
            const auto session = loginComposed(fixture);
            request = makeAuthenticatedRequest("GET", "/", session.cookie);
            TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
            const auto expectedAuthenticated =
                shell_golden::webShell(state, session.csrf, true, 1U);
            const auto split = expectedAuthenticated.find(";const E=");
            TEST_ASSERT_EQUAL_STRING(
                expectedAuthenticated.substr(0U, split).c_str(),
                shellDocument(response).substr(0U, split).c_str());
        }
        // Real shell construction sets a suffix before the real size check.
        WebRouteDispatcherTestAccess::extraShellBytes(8192U);
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        WebRouteDispatcherTestAccess::extraShellBytes(0U);
        TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
        TEST_ASSERT_EQUAL_STRING("application/json; charset=utf-8",
                                 response.contentType.c_str());
        TEST_ASSERT_EQUAL_STRING("{\"error\":\"web-shell-unavailable\"}",
                                 response.body.c_str());
        TEST_ASSERT_TRUE(response.staticBodySuffix.empty());
        // Reuse a response carrying a suffix for an unrelated error route.
        response.staticBodySuffix = "UNWANTED-HTML-JS";
        request = makeWebRequest("POST", "/");
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_TRUE(response.staticBodySuffix.empty());
    }
}

void test_asset_route_crc_cache_and_public_bytes() {
    std::string canonical;
    for (unsigned mode = 0U; mode < 4U; ++mode) {
        ComposedFixture fixture;
        if (mode != 0U)
            TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
                fixture.application, passwordUnderTest(), kPinUnderTest));
        if (mode == 2U)
            TEST_ASSERT_TRUE(
                FermentationApplicationTestAccess::setWebPasswordEnabled(
                    fixture.application, false));
        if (mode == 3U)
            TEST_ASSERT_TRUE(FermentationApplicationTestAccess::forceRootState(
                fixture.application, AuthProvisioningState::RecoveryRequired));
        // CRC derived independently from the actual public bytes.
        char path[24]{};
        const std::string bytes(kWebApplicationAsset);
        std::snprintf(
            path, sizeof(path), "/assets/app.%08x.js",
            static_cast<unsigned>(device_platform::computeCrc32IsoHdlc(bytes)));
        auto request = makeWebRequest("GET", path);
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
        TEST_ASSERT_TRUE(response.body.empty());
        TEST_ASSERT_EQUAL_STRING(bytes.c_str(), completeBody(response).c_str());
        TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
        TEST_ASSERT_TRUE(response.metadata.cache ==
                         device_platform::HttpResponseCache::ImmutableAsset);
        if (canonical.empty()) canonical = completeBody(response);
        TEST_ASSERT_EQUAL_STRING(canonical.c_str(),
                                 completeBody(response).c_str());
        for (const auto& wrong :
             {std::string(path) + "?v=1",
              std::string("/assets/app.00000000.js"),
              std::string("/assets/APP.js"), std::string(path) + "/x"}) {
            request.path = wrong;
            response = {};
            TEST_ASSERT_FALSE(fixture.http.routes()->handle(request, response));
            TEST_ASSERT_TRUE(response.metadata.cache ==
                             device_platform::HttpResponseCache::None);
        }
        request.path = path;
        request.method = "POST";
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(405U, response.statusCode);
        TEST_ASSERT_TRUE(response.staticBodySuffix.empty());
        TEST_ASSERT_TRUE(response.metadata.cache ==
                         device_platform::HttpResponseCache::None);
        request.method = "GET";
        request.metadata.host = "invalid\r\n";
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);
        request.metadata.host = "esp.local";
        request.body = "x";
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);
    }
}

void test_independent_shell_and_asset_boundaries() {
    for (bool setup : {true, false}) {
        ComposedFixture fixture;
        if (!setup)
            TEST_ASSERT_TRUE(FermentationApplicationTestAccess::provision(
                fixture.application, passwordUnderTest(), kPinUnderTest));
        auto request = makeWebRequest("GET", "/");
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        const auto size = completeBody(response).size();
        const std::size_t limit = setup ? 4096U : 2048U;
        TEST_ASSERT_TRUE(size + 128U + (setup ? 0U : 19U) <= limit);
        WebRouteDispatcherTestAccess::extraShellBytes(limit - size);
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
        TEST_ASSERT_EQUAL_UINT32(limit, completeBody(response).size());
        WebRouteDispatcherTestAccess::extraShellBytes(limit - size + 1U);
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        WebRouteDispatcherTestAccess::extraShellBytes(0U);
        TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
        TEST_ASSERT_EQUAL_STRING("{\"error\":\"web-shell-unavailable\"}",
                                 response.body.c_str());
        TEST_ASSERT_EQUAL_STRING("application/json; charset=utf-8",
                                 response.contentType.c_str());
        TEST_ASSERT_TRUE(response.staticBodySuffix.empty());
    }
    ComposedFixture fixture;
    char path[24]{};
    const std::string bytes(kWebApplicationAsset);
    std::snprintf(
        path, sizeof(path), "/assets/app.%08x.js",
        static_cast<unsigned>(device_platform::computeCrc32IsoHdlc(bytes)));
    auto request = makeWebRequest("GET", path);
    device_platform::HttpResponse response;
    WebRouteDispatcherTestAccess::extraAssetBytes(16384U - bytes.size());
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_EQUAL_UINT32(16384U, completeBody(response).size());
    WebRouteDispatcherTestAccess::extraAssetBytes(16384U - bytes.size() + 1U);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    WebRouteDispatcherTestAccess::extraAssetBytes(0U);
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"web-asset-unavailable\"}",
                             response.body.c_str());
    TEST_ASSERT_EQUAL_STRING("application/json; charset=utf-8",
                             response.contentType.c_str());
    TEST_ASSERT_TRUE(response.staticBodySuffix.empty());
    TEST_ASSERT_TRUE(response.metadata.cache ==
                     device_platform::HttpResponseCache::None);
}

void test_program_detail_route_contract_and_session_gate() {
    ComposedFixture fixture;
    auto request = makeWebRequest("GET", "/api/v1/programs/test");
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    const auto session = composedRunSession(fixture);
    const auto source = fixture.application.uiPresentationSource();
    TEST_ASSERT_TRUE(source.has_value());
    const auto id = source->programCatalog.programs.front().program.id;
    request = makeWebRequest("GET", "/api/v1/programs/" + id);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);
    request = makeAuthenticatedRequest("GET", "/api/v1/programs/" + id,
                                       session.web.cookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(
        contains(response.body, ("\"id\":\"" + id + "\"").c_str()));
    TEST_ASSERT_TRUE(contains(response.body, "\"catalogRevision\":\""));
    TEST_ASSERT_TRUE(contains(response.body, "\"defaultSensorMode\":"));
    for (const auto& invalid :
         {std::string(), std::string(49U, 'a'), std::string("UPPER"),
          id + "?x=1", id + "/extra"}) {
        request.path = "/api/v1/programs/" + invalid;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);
        TEST_ASSERT_EQUAL_STRING("{\"error\":\"invalid-program-id\"}",
                                 response.body.c_str());
    }
    request.path = "/api/v1/programs/" + std::string(48U, 'z');
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(404U, response.statusCode);
    request.path = "/api/v1/programs/a";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(404U, response.statusCode);
    request.method = "POST";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(405U, response.statusCode);
    request.method = "GET";
    request.body = "x";
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);
    FermentationApplication unavailableApplication;
    WebReadOnlyApiHandler api(unavailableApplication);
    request.body.clear();
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
}

void test_program_detail_projection_bounds_nulls_and_completion_binding() {
    ComposedFixture fixture;
    const auto read = fixture.application.uiPresentationSource();
    TEST_ASSERT_TRUE(read.has_value());
    auto source = *read;
    auto entry = source.programCatalog.programs.front();
    entry.program.id = std::string(48U, 'p');
    entry.program.name.clear();
    for (unsigned i = 0; i < 48U; ++i) entry.program.name += "é";
    source.programCatalog.programs.assign(16U, entry);
    source.programCatalogRevision = ProgramCatalogRevision{UINT64_MAX};
    std::string body;
    TEST_ASSERT_TRUE(encodeWebApiProgram(source, entry, body));
    TEST_ASSERT_TRUE(body.size() <= 3072U);
    TEST_ASSERT_TRUE(
        contains(body, "\"catalogRevision\":\"18446744073709551615\""));
    TEST_ASSERT_FALSE(contains(body, "notes"));
    TEST_ASSERT_FALSE(contains(body, "fermentationStages"));
    entry.program.name = std::string(48U, '"');
    TEST_ASSERT_TRUE(encodeWebApiProgram(source, entry, body));
    TEST_ASSERT_TRUE(contains(body, "\\\""));
    entry.program.fermentationStages.clear();
    TEST_ASSERT_TRUE(encodeWebApiProgram(source, entry, body));
    TEST_ASSERT_TRUE(contains(body, "\"targetCelsius\":null"));
    TEST_ASSERT_TRUE(contains(body, "\"durationMinutes\":null"));
    TEST_ASSERT_TRUE(contains(body, "\"startable\":false"));
    for (const auto& flag : {0U, 1U}) {
        auto hidden = entry;
        if (flag)
            hidden.program.installed = false;
        else
            hidden.program.enabled = false;
        TEST_ASSERT_FALSE(encodeWebApiProgram(source, hidden, body));
    }
    const std::array<CompletionMode, 4U> modes{
        CompletionMode::FinishWithoutCooling, CompletionMode::CoolThenFinish,
        CompletionMode::CoolAndHoldForDuration,
        CompletionMode::CoolAndHoldUntilManualStop};
    const std::array<const char*, 4U> display{
        "finish", "cool-finish", "cool-hold-duration", "cool-hold-manual"};
    const std::array<const char*, 4U> intent{
        "finish-without-cooling", "cool-then-finish",
        "cool-and-hold-for-duration", "cool-and-hold-until-manual-stop"};
    for (std::size_t i = 0; i < modes.size(); ++i) {
        entry.program.completion.mode = modes[i];
        entry.program.completion.coolingTargetCelsius =
            i ? std::optional<double>(5.0) : std::nullopt;
        entry.program.completion.holdDurationMinutes =
            i == 2U ? std::optional<std::uint32_t>(90U) : std::nullopt;
        TEST_ASSERT_TRUE(encodeWebApiProgram(source, entry, body));
        TEST_ASSERT_TRUE(contains(
            body, (std::string("\"mode\":\"") + display[i] + "\"").c_str()));
        WebRunMutationDto dto;
        const auto json = std::string(
                              "{\"v\":1,\"r\":{\"s\":1},\"i\":{\"t\":\"start-"
                              "program\",\"c\":{\"p\":\"test\",\"c\":\"") +
                          intent[i] + "\"" + (i ? ",\"k\":5" : "") +
                          (i == 2U ? ",\"l\":90" : "") + "}}}";
        TEST_ASSERT_TRUE(decodeWebRunMutation(json, dto) ==
                         WebRunMutationDecodeStatus::Success);
        TEST_ASSERT_TRUE(std::get<FermentationUiStartProgramIntent>(dto.intent)
                             .candidate.completionMode == modes[i]);
    }
}

std::string nameBody(UserConfigurationRevision revision,
                     const std::string& name = "New device") {
    return "{\"v\":1,\"r\":{\"u\":\"" + std::to_string(revision.value()) +
           "\"},\"i\":{\"t\":\"set-device-name\",\"n\":\"" + name + "\"}}";
}

void test_settings_decode_closed_schema_and_separate_run_budget() {
    WebMutationDto dto;
    const auto valid = nameBody(UserConfigurationRevision{7U});
    TEST_ASSERT_TRUE(decodeWebMutation(valid, dto) ==
                     WebRunMutationDecodeStatus::Success);
    TEST_ASSERT_TRUE(std::holds_alternative<WebConfigurationMutationDto>(dto));
    const std::array<const char*, 8U> invalid{
        "{\"v\":1,\"r\":{},\"i\":{\"t\":\"set-device-name\",\"n\":\"Test\"}}",
        "{\"v\":1,\"r\":{\"u\":7},\"i\":{\"t\":\"set-device-name\",\"n\":"
        "\"Test\"}}",
        "{\"v\":1,\"r\":{\"u\":\"18446744073709551616\"},\"i\":{\"t\":\"set-"
        "device-name\",\"n\":\"Test\"}}",
        "{\"v\":1,\"r\":{\"u\":\"1\",\"s\":1},\"i\":{\"t\":\"set-device-name\","
        "\"n\":\"Test\"}}",
        "{\"v\":1,\"r\":{\"u\":\"1\"},\"i\":{\"t\":\"set-device-name\",\"n\":"
        "\"Test\",\"origin\":3}}",
        "{\"v\":1,\"r\":{\"u\":\"1\"},\"i\":{\"t\":\"set-time-zone\",\"n\":"
        "\"Europe/Zurich\"}}",
        "{\"v\":1,\"r\":{\"u\":\"1x\"},\"i\":{\"t\":\"set-device-name\",\"n\":"
        "\"Test\"}}",
        "{\"v\":1,\"r\":{\"u\":\"1\",\"u\":\"2\"},\"i\":{\"t\":\"set-device-"
        "name\",\"n\":\"Test\"}}"};
    for (const auto* body : invalid)
        TEST_ASSERT_TRUE(decodeWebMutation(body, dto) ==
                         WebRunMutationDecodeStatus::Invalid);
    TEST_ASSERT_TRUE(decodeWebMutation(
                         valid + std::string(1024U - valid.size(), ' '), dto) ==
                     WebRunMutationDecodeStatus::Success);
    TEST_ASSERT_TRUE(decodeWebMutation(
                         valid + std::string(1025U - valid.size(), ' '), dto) ==
                     WebRunMutationDecodeStatus::TooLarge);
    WebRunMutationDto run;
    const std::string runJson =
        "{\"v\":1,\"r\":{\"s\":1},\"i\":{\"t\":\"ack-message\",\"id\":7}}";
    TEST_ASSERT_TRUE(
        decodeWebRunMutation(runJson + std::string(481U - runJson.size(), ' '),
                             run) == WebRunMutationDecodeStatus::TooLarge);
}

void test_settings_all_preview_and_commit_outcomes() {
    using P = ConfigurationPreviewStatus;
    using C = ConfigurationCommitStatus;
    using O = ReplayOutcomeCode;
    const std::array<std::pair<P, O>, 7U> previews{
        {{P::ConfigurationRuntimeUnavailable, O::Unavailable},
         {P::ConfigurationModelBudgetBusy, O::Busy},
         {P::InvalidCandidate, O::Rejected},
         {P::NotAllowed, O::Rejected},
         {P::StateChanged, O::Stale},
         {P::PreviewNotFound, O::Unavailable},
         {P::PreviewSuperseded, O::Busy}}};
    const std::array<std::pair<C, O>, 12U> commits{
        {{C::Activated, O::Applied},
         {C::NoChange, O::Applied},
         {C::PreviewNotFound, O::Unavailable},
         {C::PreviewSuperseded, O::Busy},
         {C::ConfigurationMutationBusy, O::Busy},
         {C::ConfigurationConflictFailure, O::Stale},
         {C::ConfigurationValidationFailure, O::Rejected},
         {C::PersistenceFailure, O::WriteFailed},
         {C::CapacityFailure, O::WriteFailed},
         {C::ConfigurationCommitIndeterminate, O::Unavailable},
         {C::ConfigurationRuntimeFailure, O::Unavailable},
         {C::ReadyForConfirmation, O::Unavailable}}};
    for (const auto& [preview, expected] : previews)
        for (const auto& [commit, unused] : commits) {
            static_cast<void>(unused);
            TEST_ASSERT_TRUE(WebRunMutationHandlerTestAccess::projectConfig(
                                 {preview, commit}) == expected);
        }
    for (const auto& [commit, expected] : commits)
        TEST_ASSERT_TRUE(WebRunMutationHandlerTestAccess::projectConfig(
                             {P::Success, commit}) == expected);
    TEST_ASSERT_TRUE(WebRunMutationHandlerTestAccess::projectConfig(
                         {static_cast<P>(255U), C::Activated}) ==
                     O::Unavailable);
    TEST_ASSERT_TRUE(WebRunMutationHandlerTestAccess::projectConfig(
                         {P::Success, static_cast<C>(255U)}) == O::Unavailable);
}

void test_settings_route_gate_replay_origin_and_network_characterization() {
    ComposedFixture fixture;
    const auto session = composedRunSession(fixture);
    const auto before = fixture.application.webSettings();
    TEST_ASSERT_TRUE(before.has_value());
    const auto body = nameBody(before->revision);
    const auto manifest =
        FermentationApplicationTestAccess::manifest(fixture.application);
    const auto host = fixture.network.hostname(),
               ssid = fixture.network.accessPointSsid();
    const auto starts = fixture.network.startCallCount(),
               stops = fixture.network.stopCallCount();
    for (unsigned gate = 0U; gate < 14U; ++gate) {
        auto request =
            makeRunRequest(session.web.cookie, session.web.csrf, "1", body);
        switch (gate) {
            case 0U:
                request.method = "GET";
                break;
            case 1U:
                request.metadata.host = "bad\r\n";
                break;
            case 2U:
                request.metadata.contentType = "text/plain";
                break;
            case 3U:
                request.metadata.origin = "http://foreign.local";
                break;
            case 4U:
                request.body.clear();
                break;
            case 5U:
                request.metadata.cookie.reset();
                break;
            case 6U:
                request.metadata.csrfToken = "bad";
                break;
            case 7U:
                request.metadata.mutationSeq.reset();
                break;
            case 8U:
                request.metadata.mutationSeq = "invalid";
                break;
            case 9U:
                request.metadata.mutationSeq = "2";
                break;
            case 11U:
                request.body =
                    "{\"v\":1,\"r\":{\"u\":\"1\"},\"i\":{\"t\":\"set-time-"
                    "zone\",\"n\":\"Europe/Zurich\"}}";
                break;
            case 12U:
                request.metadata.cookie = "FSSESSION=invalid";
                break;
            case 13U:
                request.body = std::string(1025U, 'x');
                break;
            case 10U:
                request.body =
                    "{\"v\":1,\"r\":{},\"i\":{\"t\":\"set-device-name\",\"n\":"
                    "\"Test\"}}";
                break;
        }
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
        TEST_ASSERT_TRUE(response.statusCode >= 400U);
        TEST_ASSERT_EQUAL_UINT32(
            0U,
            FermentationApplicationTestAccess::webCalls(fixture.application));
        TEST_ASSERT_TRUE(
            FermentationApplicationTestAccess::manifest(fixture.application)
                .version == manifest.version);
    }
    const auto applied = postRun(fixture, session, 1U, body);
    TEST_ASSERT_EQUAL_UINT16(200U, applied.statusCode);
    TEST_ASSERT_EQUAL_STRING("{\"outcome\":\"applied\"}", applied.body.c_str());
    TEST_ASSERT_EQUAL_UINT32(
        1U, FermentationApplicationTestAccess::webCalls(fixture.application));
    const auto after =
        FermentationApplicationTestAccess::manifest(fixture.application);
    TEST_ASSERT_TRUE(after.version != manifest.version);
    ConfigurationGraphStore graphStore(fixture.store, fixture.timeZoneResolver);
    const auto persisted = graphStore.loadCanonicalGraph(after.storageEpoch);
    TEST_ASSERT_TRUE(persisted.graph.has_value());
    TEST_ASSERT_TRUE(persisted.graph->active.manifest.origin ==
                     decodeChangeOrigin(3U));
    TEST_ASSERT_TRUE(persisted.graph->active.manifest.origin.kind ==
                     ChangeOriginKind::WebInterface);
    TEST_ASSERT_EQUAL_STRING(host.c_str(), fixture.network.hostname().c_str());
    TEST_ASSERT_EQUAL_STRING(ssid.c_str(),
                             fixture.network.accessPointSsid().c_str());
    TEST_ASSERT_EQUAL_UINT32(starts, fixture.network.startCallCount());
    TEST_ASSERT_EQUAL_UINT32(stops, fixture.network.stopCallCount());
    const auto replay = postRun(fixture, session, 1U, body);
    TEST_ASSERT_EQUAL_STRING(applied.body.c_str(), replay.body.c_str());
    TEST_ASSERT_EQUAL_UINT32(
        1U, FermentationApplicationTestAccess::webCalls(fixture.application));
    TEST_ASSERT_EQUAL_UINT16(
        409U,
        postRun(fixture, session, 1U, nameBody(before->revision, "Different"))
            .statusCode);
    TEST_ASSERT_EQUAL_UINT32(
        1U, FermentationApplicationTestAccess::webCalls(fixture.application));
    const auto current = fixture.application.webSettings();
    TEST_ASSERT_EQUAL_UINT16(
        200U,
        postRun(fixture, session, 2U, nameBody(current->revision, "New device"))
            .statusCode);
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::manifest(fixture.application)
            .version == after.version);
    TEST_ASSERT_EQUAL_UINT16(
        409U, postRun(fixture, session, 3U, nameBody(before->revision, "Stale"))
                  .statusCode);
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::manifest(fixture.application)
            .version == after.version);
    auto request =
        makeAuthenticatedRequest("GET", "/api/v1/settings", session.web.cookie);
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(
        200U, response.statusCode);  // Session survives name change.
    TEST_ASSERT_TRUE(contains(response.body, "\"timeZone\":\"Europe/Zurich\""));
    TEST_ASSERT_TRUE(contains(response.body, "\"nameEditable\":true"));
    const auto touch = fixture.application.applyUserSettings(
        FermentationUiUserSettingsChange{std::string("Touch name")},
        fixture.application.webSettings()->revision);
    TEST_ASSERT_TRUE(touch.commit == ConfigurationCommitStatus::Activated);
    const auto touched =
        FermentationApplicationTestAccess::manifest(fixture.application);
    const auto touchGraph = graphStore.loadCanonicalGraph(touched.storageEpoch);
    TEST_ASSERT_TRUE(touchGraph.graph->active.manifest.origin ==
                     decodeChangeOrigin(2U));
    const auto maxBody =
        nameBody(fixture.application.webSettings()->revision, "Touch name");
    const auto padded = maxBody + std::string(1024U - maxBody.size(), ' ');
    TEST_ASSERT_EQUAL_UINT16(200U,
                             postRun(fixture, session, 4U, padded).statusCode);
    const auto calls =
        FermentationApplicationTestAccess::webCalls(fixture.application);
    TEST_ASSERT_EQUAL_UINT16(200U,
                             postRun(fixture, session, 4U, padded).statusCode);
    TEST_ASSERT_EQUAL_UINT32(calls, FermentationApplicationTestAccess::webCalls(
                                        fixture.application));
    TEST_ASSERT_EQUAL_UINT16(
        413U, postRun(fixture, session, 5U, padded + " ").statusCode);
    TEST_ASSERT_EQUAL_UINT32(calls, FermentationApplicationTestAccess::webCalls(
                                        fixture.application));
    installActiveRunForStatus(fixture.application, "Active");
    TEST_ASSERT_FALSE(fixture.application.webSettings()->nameEditable);
    const auto rejected = postRun(
        fixture, session, 5U,
        nameBody(fixture.application.webSettings()->revision, "Forbidden"));
    TEST_ASSERT_EQUAL_UINT16(422U, rejected.statusCode);
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::manifest(fixture.application)
            .version == touched.version);
}

void test_new_handler_allocation_windows_and_final_budgets() {
    ComposedFixture fixture;
    const auto session = composedRunSession(fixture);
    const auto source = fixture.application.uiPresentationSource();
    TEST_ASSERT_TRUE(source.has_value());
    const auto& id = source->programCatalog.programs.front().program.id;
    char asset[24]{};
    std::snprintf(asset, sizeof(asset), "/assets/app.%08x.js",
                  static_cast<unsigned>(device_platform::computeCrc32IsoHdlc(
                      kWebApplicationAsset)));
    for (const auto& path :
         {std::string("/"), std::string(asset), std::string("/api/v1/settings"),
          std::string("/api/v1/programs/") + id})
        for (unsigned pass = 0; pass < 2; ++pass) {
            const auto request =
                makeAuthenticatedRequest("GET", path, session.web.cookie);
            device_platform::HttpResponse response;
            bool handled = false;
            const auto w = alloc_window::measure([&] {
                handled = fixture.http.routes()->handle(request, response);
            });
            TEST_ASSERT_TRUE(handled);
            TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
            TEST_ASSERT_EQUAL_UINT32(0U, w.cJsonLiveBytes);
            if (path == asset) TEST_ASSERT_TRUE(response.body.empty());
            std::printf(
                "PR209_ALLOC %s pass=%u count=%zu bytes=%zu largest=%zu "
                "peak_live=%zu cjson_count=%zu body=%zu\n",
                path.c_str(), pass, w.count, w.requestedBytes, w.largestBlock,
                w.peakLiveBytes, w.cJsonCount, completeBody(response).size());
        }
    const auto shell = rawShell(fixture, session.web.cookie);
    TEST_ASSERT_TRUE(shell.size() + 19U + 128U <= 2048U);
    TEST_ASSERT_TRUE(contains(shell, "nav[hidden]{display:none}"));
    TEST_ASSERT_TRUE(sizeof(kWebApplicationAsset) - 1U + 1024U <= 16384U);
    std::printf(
        "PR209_FINAL_BYTES shell=%zu shell_max_sequence=%zu asset=%zu "
        "asset_reserve=%zu\n",
        shell.size(), shell.size() + 19U, sizeof(kWebApplicationAsset) - 1U,
        16384U - (sizeof(kWebApplicationAsset) - 1U));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_new_handler_allocation_windows_and_final_budgets);
    RUN_TEST(test_settings_decode_closed_schema_and_separate_run_budget);
    RUN_TEST(test_settings_all_preview_and_commit_outcomes);
    RUN_TEST(
        test_settings_route_gate_replay_origin_and_network_characterization);
    RUN_TEST(test_program_detail_route_contract_and_session_gate);
    RUN_TEST(
        test_program_detail_projection_bounds_nulls_and_completion_binding);
    RUN_TEST(test_asset_route_crc_cache_and_public_bytes);
    RUN_TEST(test_independent_shell_and_asset_boundaries);
    RUN_TEST(
        test_split_shell_matches_frozen_generator_and_clears_suffix_on_errors);
    RUN_TEST(test_outcome_matrix_accepts_only_owning_apply_results);
    RUN_TEST(test_outcome_matrix_conflicts_busy_and_stale_are_409);
    RUN_TEST(test_outcome_matrix_rejections_and_capacity_are_typed);
    RUN_TEST(test_outcome_matrix_recovery_and_indeterminate_states_fail_closed);
    RUN_TEST(test_application_request_status_mapping_is_fail_closed);
    RUN_TEST(test_handler_requires_session_csrf_same_origin_and_json);
    RUN_TEST(test_handler_runs_prepare_confirm_apply_and_replays_exact_result);
    RUN_TEST(test_handler_applies_ram_owned_ack_without_run_persistence);
    RUN_TEST(test_handler_applies_ram_owned_mute_without_run_persistence);
    RUN_TEST(
        test_handler_maps_stale_revision_and_invalid_program_without_mutation);
    RUN_TEST(
        test_handler_refuses_manual_runs_and_cooling_plans_without_a_limits_owner);
    RUN_TEST(test_mutation_codec_rejects_invalid_bodies_without_partial_dto);
    RUN_TEST(test_mutation_codec_decodes_every_closed_application_intent);
    RUN_TEST(test_mutation_codec_validates_static_field_ranges_and_completion);
    RUN_TEST(
        test_mutation_codec_rejects_invalid_manual_plans_and_variant_shapes);
    RUN_TEST(test_mutation_codec_uses_canonical_decimal_revision_strings);
    RUN_TEST(test_maximum_product_mutation_fits_body_and_exact_replay_budget);
    RUN_TEST(test_read_only_api_projection_bounds_and_untrusted_values);
    RUN_TEST(test_read_only_api_routes_are_get_only_and_uncomposed);
    RUN_TEST(
        test_composed_dispatcher_preserves_setup_priority_and_single_server);
    RUN_TEST(test_composed_dispatcher_revokes_sessions_on_network_boundaries);
    RUN_TEST(test_composed_dispatcher_keeps_sessions_on_failed_network_change);
    RUN_TEST(
        test_composed_dispatcher_revokes_sessions_for_home_wifi_reconfiguration);
    RUN_TEST(test_composed_dispatcher_factory_reset_revokes_old_sessions);
    RUN_TEST(
        test_composed_dispatcher_anonymous_and_recovery_states_fail_closed);
    RUN_TEST(
        test_composed_dispatcher_authenticates_read_only_sessions_and_expires);
    RUN_TEST(
        test_application_call_serializer_blocks_cross_thread_and_allows_reentry);
    RUN_TEST(test_factory_reset_drains_running_login_before_touching_the_store);
    RUN_TEST(test_login_after_gate_close_is_fail_closed_without_kdf);
    RUN_TEST(test_failed_factory_reset_reopens_the_gate_and_keeps_the_domain);
    RUN_TEST(test_reset_may_proceed_right_after_the_domain_returns);
    RUN_TEST(
        test_local_pin_check_and_factory_reset_neither_hang_nor_grant_access);
    RUN_TEST(test_local_pin_check_after_a_failed_reset_still_authorizes);
    RUN_TEST(test_local_pin_check_after_gate_close_is_unavailable_without_kdf);
    RUN_TEST(test_local_pin_check_refuses_access_after_a_same_epoch_reinit);
    RUN_TEST(test_existing_local_lease_survives_a_network_trust_boundary);
    RUN_TEST(
        test_stale_protected_login_cannot_create_a_session_after_factory_reset);
    RUN_TEST(test_protected_login_session_created_before_reset_is_revoked);
    RUN_TEST(
        test_stale_disabled_mode_login_cannot_create_a_session_after_reset);
    RUN_TEST(test_stale_login_cannot_create_a_session_after_network_boundaries);
    RUN_TEST(
        test_concurrent_login_and_factory_reset_never_leave_a_valid_session);
    RUN_TEST(test_window_opens_only_when_unprovisioned_and_bootstrap_allowed);
    RUN_TEST(test_window_lasts_exactly_ten_minutes_and_is_not_extended);
    RUN_TEST(test_window_fails_closed_on_backward_clock_observation);
    RUN_TEST(test_window_closes_at_boundaries_and_survives_candidate_commit);
    RUN_TEST(
        test_window_closes_lazily_when_auth_state_is_no_longer_bootstrap_allowed);
    RUN_TEST(test_provision_protect_consumes_window_and_login_works);
    RUN_TEST(
        test_provision_disable_uses_only_service_pin_kdf_and_allows_sessions);
    RUN_TEST(test_provision_without_window_is_not_allowed_without_kdf_or_write);
    RUN_TEST(test_provision_state_matrix_never_calls_bootstrap);
    RUN_TEST(test_provision_input_and_failure_contracts);
    RUN_TEST(test_provision_codec_accepts_exactly_the_two_schemas);
    RUN_TEST(test_provision_codec_body_limit_and_whitespace);
    RUN_TEST(
        test_provision_codec_rejects_every_schema_violation_without_partial_dto);
    RUN_TEST(test_provision_route_request_security_contract);
    RUN_TEST(test_provision_route_without_window_changes_nothing);
    RUN_TEST(test_provision_route_protect_success_creates_no_session);
    RUN_TEST(test_provision_route_disable_success_creates_no_session);
    RUN_TEST(test_provision_route_credential_errors_are_422_and_retryable);
    RUN_TEST(test_provision_route_recovery_and_failure_mapping);
    RUN_TEST(test_provision_route_is_not_served_during_setup_flow);
    RUN_TEST(test_run_route_is_gated_by_authentication_state);
    RUN_TEST(test_run_route_is_composed_with_session_csrf_and_replay);
    RUN_TEST(
        test_dispatcher_run_ack_and_mute_apply_once_and_replay_without_second_effect);
    RUN_TEST(test_dispatcher_run_two_sessions_same_revision_yield_one_effect);
    RUN_TEST(test_dispatcher_run_local_change_between_status_and_post_is_409);
    RUN_TEST(
        test_dispatcher_run_start_program_is_fail_closed_and_changes_nothing);
    RUN_TEST(test_dispatcher_run_limits_gated_actions_stay_503);
    RUN_TEST(test_dispatcher_run_mutation_sequence_contract);
    RUN_TEST(test_programs_route_gate_and_request_contract);
    RUN_TEST(test_programs_route_lists_catalog_names_with_the_status_revision);
    RUN_TEST(test_programs_encoder_filters_empty_and_bounded);
    RUN_TEST(test_programs_route_without_catalog_source_is_503_not_empty);
    RUN_TEST(test_shell_start_flow_text_contract);
    RUN_TEST(
        test_unprovisioned_shell_shows_the_setup_form_within_the_size_limit);
    RUN_TEST(
        test_language_selector_is_in_every_shell_state_without_server_state);
    RUN_TEST(
        test_language_choice_never_reaches_the_api_or_the_polling_contract);
    RUN_TEST(test_shell_run_controls_text_contract);
    RUN_TEST(test_shell_action_availability_and_confirmation_binding_contract);
    RUN_TEST(test_shell_embeds_mutation_sequence_per_session);
    RUN_TEST(test_shell_login_reload_bootstrap_for_protected_and_disabled);
    RUN_TEST(test_shell_stays_within_the_limit_in_all_states);
    RUN_TEST(test_dashboard_shell_structure_and_responsive_contract);
    RUN_TEST(test_dashboard_shell_texts_are_complete_in_all_locales);
    RUN_TEST(test_dashboard_shell_labels_cover_every_codec_token);
    RUN_TEST(test_shell_run_information_is_bound_to_the_status_codec);
    RUN_TEST(test_shell_device_info_is_bound_to_the_status_codec);
    RUN_TEST(test_device_info_view_reports_name_zone_local_time_and_connection);
    RUN_TEST(test_device_info_connection_requires_the_configured_network_mode);
    RUN_TEST(test_status_endpoint_adds_device_info_without_network_secrets);
    RUN_TEST(test_status_codec_device_info_tokens_and_unavailable_parts);
    RUN_TEST(
        test_status_handler_allocation_window_covers_snapshot_encode_and_response);
    RUN_TEST(test_dashboard_feed_fields_in_standby_and_constructed_states);
    RUN_TEST(test_dashboard_missing_and_stale_data_contract);
    RUN_TEST(test_dashboard_ordering_guard_and_age_contract_in_the_shell_text);
    RUN_TEST(test_unprovisioned_shell_requests_match_the_s4_contract);
    RUN_TEST(
        test_unprovisioned_shell_explains_local_release_and_creates_no_session);
    RUN_TEST(test_unprovisioned_shell_texts_are_complete_in_all_locales);
    RUN_TEST(test_other_states_get_no_provisioning_form_or_bypass);
    RUN_TEST(test_no_http_route_reaches_the_local_factory_reset);
    RUN_TEST(test_provision_commit_outcome_unknown_closes_window_immediately);
    RUN_TEST(test_provision_lost_race_closes_window_via_current_state);
    RUN_TEST(test_provision_result_projection_is_fail_closed);
    RUN_TEST(test_provision_gate_closed_is_fail_closed_without_kdf);
    RUN_TEST(
        test_provision_running_during_factory_reset_is_not_reported_current);
    RUN_TEST(
        test_concurrent_provisioning_has_one_winner_and_one_credential_write);
    RUN_TEST(test_destructor_drains_a_running_login);
    RUN_TEST(test_auth_operation_gate_object_size_is_reported);
    return UNITY_END();
}
