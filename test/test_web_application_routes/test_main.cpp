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
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>

#include "device_platform.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "connectivity_credentials.hpp"
#include "fermentation_application.hpp"
#include "mock_time_zone_resolver.hpp"
#include "mock_network_lifecycle.hpp"
#include "program_limits.hpp"
#include "mock_secure_random_source.hpp"
#include "simulated_persistent_state_store.hpp"
#include "virtual_time_source.hpp"
#include "web_application_routes.hpp"
#include "web_json_codec.hpp"

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

namespace {

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

   private:
    bool running_{false};
    std::size_t startCount_{0U};
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
                                           kdf));
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
    const auto start = response.body.find(marker);
    TEST_ASSERT_TRUE(start != std::string::npos);
    const auto valueStart = start + sizeof(marker) - 1U;
    const auto end = response.body.find('"', valueStart);
    TEST_ASSERT_TRUE(end != std::string::npos);
    return response.body.substr(valueStart, end - valueStart);
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
    TEST_ASSERT_TRUE(response.body.find("authentication-unavailable") !=
                     std::string::npos);
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

ManualTimedRunValues validManualTimedValues() {
    ManualTimedRunValues values;
    values.targetTemperatureCelsius = 30.0;
    values.durationMinutes = 60U;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;
    return values;
}

WebRunMutationDto startRequest(const FermentationApplication& application) {
    return {application.uiSnapshot().revisions,
            FermentationUiStartManualTimedIntent{validManualTimedValues()}};
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
                             response.body.c_str());

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
    const auto dto = startRequest(fixture.application);
    const auto request = makeRequest(fixture, dto);
    device_platform::HttpResponse first;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, first));
    TEST_ASSERT_EQUAL_UINT16(200U, first.statusCode);
    TEST_ASSERT_EQUAL_STRING("{\"outcome\":\"applied\"}", first.body.c_str());
    const auto afterFirst = fixture.application.uiSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::ActiveRun),
                          static_cast<int>(afterFirst.home.mode));

    device_platform::HttpResponse replay;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, replay));
    TEST_ASSERT_EQUAL_UINT16(first.statusCode, replay.statusCode);
    TEST_ASSERT_EQUAL_STRING(first.contentType.c_str(),
                             replay.contentType.c_str());
    TEST_ASSERT_EQUAL_STRING(first.body.c_str(), replay.body.c_str());
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
    TEST_ASSERT_TRUE(encodeWebApiStatus(snapshot, status));
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
    TEST_ASSERT_TRUE(response.body.size() <= kMaximumWebApiResponseBodyBytes);

    request.path = "/api/v1/temperatures";
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(response.body.find("temperatures") != std::string::npos);

    request.path = "/api/v1/alerts";
    TEST_ASSERT_TRUE(api.handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(response.body.find("alerts") != std::string::npos);

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
    TEST_ASSERT_TRUE(response.body.find("Fermentation") != std::string::npos);

    request = makeWebRequest("GET", "/api/v1/status");
    TEST_ASSERT_TRUE(routes->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
    request = makeWebRequest("POST", "/internal/ui/run");
    TEST_ASSERT_FALSE(routes->handle(request, response));

    const auto home = fixture.application.applyNetworkMode(
        device_platform::NetworkMode::HOME_WIFI);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(home.status));
    TEST_ASSERT_TRUE(fixture.application.networkSetupFlowActive());
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.http.startCount());

    request = makeWebRequest("GET", "/");
    TEST_ASSERT_TRUE(routes->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(response.body.find("Network setup") != std::string::npos);

    request = makeWebRequest("GET", "/api/v1/status");
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
    TEST_ASSERT_TRUE(response.body.find("candidate committed") !=
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
    TEST_ASSERT_TRUE(response.body.find("authenticated") == std::string::npos);
    if (postResetAuth == WebAuthenticationState::Unprovisioned) {
        TEST_ASSERT_TRUE(response.body.find("provisioning-required") !=
                         std::string::npos);
    } else {
        TEST_ASSERT_TRUE(response.body.find("recovery-required") !=
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
    TEST_ASSERT_TRUE(
        response.body.find("Local provisioning or recovery required") !=
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
    TEST_ASSERT_TRUE(response.body.find("Password protection is disabled") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(response.body.find("password-label") != std::string::npos);
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
    TEST_ASSERT_TRUE(response.body.find("invalid-credentials") !=
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
    TEST_ASSERT_TRUE(response.body.find("const U=true") != std::string::npos);
    TEST_ASSERT_TRUE(
        response.body.find("document.documentElement.lang=locale") !=
        std::string::npos);
    TEST_ASSERT_TRUE(response.body.find("noSnapshot") != std::string::npos);

    request = makeAuthenticatedRequest("GET", "/api/v1/status", firstCookie);
    TEST_ASSERT_TRUE(fixture.http.routes()->handle(request, response));
    TEST_ASSERT_EQUAL_UINT16(200U, response.statusCode);
    TEST_ASSERT_TRUE(response.body.find("password") == std::string::npos);
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
    TEST_ASSERT_TRUE(response.body.find(error) != std::string::npos);
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
        response.body ==
        "{\"provisioned\":true,\"passwordProtection\":\"enabled\"}");
    TEST_ASSERT_FALSE(response.metadata.setCookie.has_value());
    TEST_ASSERT_TRUE(response.body.find("csrf") == std::string::npos);
    TEST_ASSERT_TRUE(response.body.find(passwordUnderTest()) ==
                     std::string::npos);
    TEST_ASSERT_TRUE(response.body.find(kPinUnderTest) == std::string::npos);
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
        response.body ==
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

void test_provision_route_does_not_open_the_run_mutation_route() {
    ComposedFixture fixture;
    auto request = makeWebRequest("POST", "/internal/ui/run");
    device_platform::HttpResponse response;
    TEST_ASSERT_FALSE(fixture.http.routes()->handle(request, response));
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

int main() {
    UNITY_BEGIN();
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
    RUN_TEST(test_provision_route_does_not_open_the_run_mutation_route);
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
