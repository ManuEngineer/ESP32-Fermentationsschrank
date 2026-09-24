#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include <unity.h>

#include "authentication_records.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "mock_time_zone_resolver.hpp"
#include "run_persistence_coordinator.hpp"
#include "simulated_persistent_state_store.hpp"
#include "virtual_time_source.hpp"
#include "web_application_routes.hpp"

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
    static bool applicationReadiness(
        const FermentationApplication& application) {
        return application.applicationReadiness();
    }

    static std::uint32_t stateSequence(
        const FermentationApplication& application) {
        return application.runtimeRunState_->processState.transitionSequence;
    }

    static void addAcknowledgableMessage(FermentationApplication& application,
                                         std::uint32_t id) {
        auto& state = *application.runtimeRunState_;
        state.messages[0] = {id,
                             MessageCode::RunCompleted,
                             MessageClass::Information,
                             0U,
                             MessageTrigger::Process,
                             0U,
                             true,
                             false,
                             false,
                             false,
                             false,
                             AcousticIntent::None,
                             std::nullopt,
                             std::nullopt,
                             std::nullopt,
                             1U};
        state.messageCount = 1U;
        state.messageRevision = 1U;
    }

    static RunPersistenceCoordinator& runPersistenceCoordinator(
        FermentationApplication& application) {
        return *application.runPersistenceCoordinator_;
    }
};

}  // namespace fermentation

namespace {

class Random final : public device_platform::ISecureRandomSource {
   public:
    bool fill(void* buffer, std::size_t length) override {
        if (buffer == nullptr && length != 0U) return false;
        auto* bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t i = 0U; i < length; ++i) bytes[i] = next_++;
        return true;
    }

   private:
    std::uint8_t next_{1U};
};

class Kdf final : public fermentation::IAuthenticationKdf {
   public:
    bool derive(
        const std::string&, const fermentation::AuthVerifier& verifier,
        std::array<std::uint8_t, fermentation::kAuthenticationVerifierBytes>&
            out) override {
        if (!verifier.valid()) return false;
        out.fill(0U);
        return true;
    }
};

fermentation::CrossRolePlausibilityContext owningEvidence() {
    fermentation::CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

device_platform::HttpRequestMetadata browserMetadata(
    const fermentation::WebSessionResult& session, std::uint64_t sequence) {
    device_platform::HttpRequestMetadata metadata;
    metadata.host = "fermentation.local";
    metadata.contentType = "application/json";
    metadata.cookie = "FSSESSION=" + session.cookieValue;
    metadata.csrfToken = session.csrfToken;
    metadata.mutationSeq = std::to_string(sequence);
    metadata.origin = "http://fermentation.local";
    metadata.secFetchSite = "same-origin";
    return metadata;
}

struct RouteFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver resolver;
    device_platform::VirtualTimeSource time;
    fermentation::FermentationApplication application;
    Random random;
    Kdf kdf;
    fermentation::AuthenticationRecordStore records{store};
    fermentation::AuthenticationDomain authentication{records, kdf, random};
    fermentation::WebSessionManager sessions{random};
    fermentation::WebApplicationRoutes routes{application, authentication,
                                              sessions, time};
    fermentation::WebSessionResult session;

    RouteFixture() {
        time.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, resolver, time));
        application.publishOwningRuntimeEvidence(owningEvidence());
        TEST_ASSERT_TRUE(fermentation::FermentationApplicationTestAccess::
                             applicationReadiness(application));
        fermentation::FermentationApplicationTestAccess::
            addAcknowledgableMessage(application, 7U);
        session = sessions.create(0U);
        TEST_ASSERT_TRUE(session.handle.has_value());
    }

    [[nodiscard]] std::string acknowledgeBody(
        bool confirmed,
        std::optional<std::uint32_t> expectedState = std::nullopt,
        std::uint32_t expectedMessage = 1U) const {
        return "{\"action\":\"acknowledge\",\"confirmed\":" +
               std::string(confirmed ? "true" : "false") +
               ",\"expectedStateSequence\":" +
               std::to_string(expectedState.value_or(
                   fermentation::FermentationApplicationTestAccess::
                       stateSequence(application))) +
               ",\"expectedMessageRevision\":" +
               std::to_string(expectedMessage) + ",\"messageId\":7}";
    }

    [[nodiscard]] device_platform::HttpResponse post(std::uint64_t sequence,
                                                     const std::string& body) {
        device_platform::HttpResponse response;
        TEST_ASSERT_TRUE(routes.handle({"POST", "/internal/ui/run", body,
                                        browserMetadata(session, sequence)},
                                       response));
        return response;
    }

    [[nodiscard]] std::string timedStartBody(
        bool confirmed,
        std::optional<std::uint32_t> expectedState = std::nullopt) const {
        return "{\"action\":\"start_manual_timed\",\"confirmed\":" +
               std::string(confirmed ? "true" : "false") +
               ",\"expectedStateSequence\":" +
               std::to_string(expectedState.value_or(
                   fermentation::FermentationApplicationTestAccess::
                       stateSequence(application))) +
               ",\"expectedRunRevision\":0"
               ",\"targetTemperatureCelsius\":30.0,"
               "\"durationMinutes\":60,\"sensorMode\":\"PRODUCT\","
               "\"preheatEnabled\":true,"
               "\"maximumProductWaitMinutes\":30,"
               "\"qualificationBandCelsius\":0.5,"
               "\"qualificationDurationMinutes\":10,"
               "\"maximumTargetReachMinutes\":180,"
               "\"completionMode\":\"FINISH_WITHOUT_COOLING\"}";
    }
};

void test_internal_run_route_requires_confirmation_and_replays_owning_outcome() {
    RouteFixture fixture;
    const auto body = fixture.timedStartBody(false);
    const auto unconfirmed = fixture.post(1U, body);
    TEST_ASSERT_EQUAL_UINT16(409U, unconfirmed.statusCode);
    TEST_ASSERT_TRUE(unconfirmed.body.find("confirmation_required") !=
                     std::string::npos);
    const auto beforeApply =
        fermentation::FermentationApplicationTestAccess::stateSequence(
            fixture.application);

    const auto confirmedBody = fixture.timedStartBody(true);
    const auto applied = fixture.post(2U, confirmedBody);
    TEST_ASSERT_EQUAL_UINT16(200U, applied.statusCode);
    const auto afterApply =
        fermentation::FermentationApplicationTestAccess::stateSequence(
            fixture.application);
    TEST_ASSERT_TRUE(afterApply > beforeApply);

    const auto replay = fixture.post(2U, confirmedBody);
    TEST_ASSERT_EQUAL_UINT16(applied.statusCode, replay.statusCode);
    TEST_ASSERT_EQUAL_STRING(applied.body.c_str(), replay.body.c_str());
    TEST_ASSERT_EQUAL_UINT32(
        afterApply,
        fermentation::FermentationApplicationTestAccess::stateSequence(
            fixture.application));

    const auto original = fixture.timedStartBody(true);
    const auto temperature = original.find("30.0");
    TEST_ASSERT_TRUE(temperature != std::string::npos);
    const auto changedPayload = original.substr(0U, temperature) + "31.0" +
                                original.substr(temperature + 4U);
    const auto conflict = fixture.post(2U, changedPayload);
    TEST_ASSERT_EQUAL_UINT16(409U, conflict.statusCode);
    TEST_ASSERT_EQUAL_UINT32(
        afterApply,
        fermentation::FermentationApplicationTestAccess::stateSequence(
            fixture.application));
}

void test_internal_run_route_maps_stale_and_missing_runtime_fail_closed() {
    RouteFixture fixture;
    const auto stale = fixture.post(
        1U, fixture.timedStartBody(
                true,
                fermentation::FermentationApplicationTestAccess::stateSequence(
                    fixture.application) +
                    1U));
    TEST_ASSERT_EQUAL_UINT16(409U, stale.statusCode);
    TEST_ASSERT_TRUE(stale.body.find("stale_confirmation") !=
                     std::string::npos);

    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver resolver;
    device_platform::VirtualTimeSource time;
    fermentation::FermentationApplication application;
    Random random;
    Kdf kdf;
    fermentation::AuthenticationRecordStore records(store);
    fermentation::AuthenticationDomain authentication(records, kdf, random);
    fermentation::WebSessionManager sessions(random);
    fermentation::WebApplicationRoutes routes(application, authentication,
                                              sessions, time);
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, resolver, time));
    const auto session = sessions.create(0U);
    TEST_ASSERT_TRUE(session.handle.has_value());
    const std::string start =
        "{\"action\":\"start_manual_timed\",\"confirmed\":true,"
        "\"expectedStateSequence\":0,\"targetTemperatureCelsius\":30.0,"
        "\"durationMinutes\":60,\"sensorMode\":\"AIR\","
        "\"preheatEnabled\":false,\"qualificationBandCelsius\":0.5,"
        "\"qualificationDurationMinutes\":10,"
        "\"maximumTargetReachMinutes\":180,"
        "\"completionMode\":\"FINISH_WITHOUT_COOLING\"}";
    device_platform::HttpResponse unavailable;
    TEST_ASSERT_TRUE(routes.handle(
        {"POST", "/internal/ui/run", start, browserMetadata(session, 1U)},
        unavailable));
    TEST_ASSERT_TRUE(unavailable.statusCode >= 400U);
    TEST_ASSERT_TRUE(unavailable.statusCode < 500U);
}

void test_internal_run_route_maps_persistence_and_recovery_outcomes() {
    {
        RouteFixture fixture;
        fixture.store.setNextWriteFault(
            device_platform_test_support::SimulatedPersistentStateStore::
                WriteFault::PowerCutAfterCommitBeforeReturn);
        fixture.store.failNextReadAfterWrite();
        const auto response = fixture.post(1U, fixture.timedStartBody(true));
        TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
        TEST_ASSERT_TRUE(response.body.find("commit_indeterminate") !=
                         std::string::npos);
    }
    const auto assertCoordinatorOutcome =
        [](fermentation::RunPersistenceCoordinatorState state,
           const char* code) {
            RouteFixture fixture;
            fermentation::RunPersistenceCoordinatorTestAccess::setState(
                fermentation::FermentationApplicationTestAccess::
                    runPersistenceCoordinator(fixture.application),
                state);
            const auto response =
                fixture.post(1U, fixture.acknowledgeBody(true));
            TEST_ASSERT_EQUAL_UINT16(503U, response.statusCode);
            TEST_ASSERT_TRUE(response.body.find(code) != std::string::npos);
        };
    assertCoordinatorOutcome(fermentation::RunPersistenceCoordinatorState::
                                 PersistenceCommittedApplyFailed,
                             "committed_apply_failed");
    assertCoordinatorOutcome(
        fermentation::RunPersistenceCoordinatorState::FallbackRecoveryPending,
        "recovery_pending");
    assertCoordinatorOutcome(
        fermentation::RunPersistenceCoordinatorState::BlockedIndeterminate,
        "blocked");
}

void test_internal_run_route_preserves_application_owner_and_replay_boundary() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver resolver;
    device_platform::VirtualTimeSource time;
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, resolver, time));
    application.publishOwningRuntimeEvidence(owningEvidence());

    Random random;
    Kdf kdf;
    fermentation::AuthenticationRecordStore records(store);
    fermentation::AuthenticationDomain authentication(records, kdf, random);
    fermentation::WebSessionManager sessions(random);
    const auto session = sessions.create(0U);
    TEST_ASSERT_TRUE(session.handle.has_value());
    fermentation::WebApplicationRoutes routes(application, authentication,
                                              sessions, time);

    const auto snapshot = application.uiSnapshot();
    const std::string body =
        "{\"action\":\"start_manual_timed\",\"confirmed\":true,"
        "\"expectedStateSequence\":" +
        std::to_string(snapshot.revisions.expectedStateSequence) +
        ",\"targetTemperatureCelsius\":30.0,\"durationMinutes\":60,"
        "\"sensorMode\":\"AIR\",\"preheatEnabled\":false,"
        "\"qualificationBandCelsius\":0.5,"
        "\"qualificationDurationMinutes\":10,"
        "\"maximumTargetReachMinutes\":180,"
        "\"completionMode\":\"FINISH_WITHOUT_COOLING\"}";
    device_platform::HttpResponse first;
    TEST_ASSERT_TRUE(routes.handle(
        {"POST", "/internal/ui/run", body, browserMetadata(session, 1U)},
        first));
    // The application-owned decision remains non-success; the Web route
    // cannot promote it to a mutation success.
    TEST_ASSERT_EQUAL_UINT16(422U, first.statusCode);
    TEST_ASSERT_TRUE(first.body.find("command_rejected") != std::string::npos);

    device_platform::HttpResponse retry;
    TEST_ASSERT_TRUE(routes.handle(
        {"POST", "/internal/ui/run", body, browserMetadata(session, 1U)},
        retry));
    TEST_ASSERT_EQUAL_UINT16(first.statusCode, retry.statusCode);
    TEST_ASSERT_EQUAL_STRING(first.body.c_str(), retry.body.c_str());

    const std::string changed = body.substr(0U, body.find("30.0")) + "31.0" +
                                body.substr(body.find("30.0") + 4U);
    device_platform::HttpResponse conflict;
    TEST_ASSERT_TRUE(routes.handle(
        {"POST", "/internal/ui/run", changed, browserMetadata(session, 1U)},
        conflict));
    TEST_ASSERT_EQUAL_UINT16(409U, conflict.statusCode);
}

}  // namespace

void setup() {}
void loop() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(
        test_internal_run_route_requires_confirmation_and_replays_owning_outcome);
    RUN_TEST(
        test_internal_run_route_maps_stale_and_missing_runtime_fail_closed);
    RUN_TEST(test_internal_run_route_maps_persistence_and_recovery_outcomes);
    RUN_TEST(
        test_internal_run_route_preserves_application_owner_and_replay_boundary);
    return UNITY_END();
}
