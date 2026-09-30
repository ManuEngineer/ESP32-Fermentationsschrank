#include <unity.h>

#include <cstdint>
#include <string>
#include <variant>

#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"
#include "virtual_time_source.hpp"
#include "web_application_routes.hpp"

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    static RunCommandState& runtimeState(FermentationApplication& application) {
        return *application.runtimeRunState_;
    }
};

struct WebRunMutationHandlerTestAccess {
    static WebMutationOutcome project(
        const FermentationUiCommandResult& result) {
        return WebRunMutationHandler::projectCommandResult(result);
    }

    static WebMutationOutcome projectRequest(
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
        if (buffer == nullptr) return false;
        auto* bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t index = 0U; index < length; ++index) {
            bytes[index] = next_++;
        }
        return true;
    }

   private:
    std::uint8_t next_{1U};
};

FermentationUiCommandResult commandResult(Category category,
                                          FermentationUiCommandDetail detail,
                                          FermentationUiCommandPhase phase) {
    return {category, std::move(detail), phase, std::nullopt, std::nullopt};
}

void assertProjected(const FermentationUiCommandResult& input,
                     std::uint16_t expectedStatus) {
    const auto projected = WebRunMutationHandlerTestAccess::project(input);
    TEST_ASSERT_EQUAL_UINT16(expectedStatus, projected.statusCode);
    TEST_ASSERT_EQUAL_STRING("application/json; charset=utf-8",
                             projected.contentType.c_str());
    TEST_ASSERT_TRUE(projected.body.size() <= kMaximumReplayOutcomeBodyBytes);
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
        const auto projected =
            WebRunMutationHandlerTestAccess::projectRequest(status);
        TEST_ASSERT_EQUAL_UINT16(409U, projected.statusCode);
    }
    for (const auto status :
         {FermentationApplicationRequestStatus::ProgramUnavailable,
          FermentationApplicationRequestStatus::InvalidInput}) {
        const auto projected =
            WebRunMutationHandlerTestAccess::projectRequest(status);
        TEST_ASSERT_EQUAL_UINT16(422U, projected.statusCode);
    }
    for (const auto status :
         {FermentationApplicationRequestStatus::NotInitialized,
          FermentationApplicationRequestStatus::Unavailable,
          FermentationApplicationRequestStatus::Overflow}) {
        const auto projected =
            WebRunMutationHandlerTestAccess::projectRequest(status);
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
    WebSessionManager sessions;
    WebRunMutationHandler handler;
    WebSessionResult session;

    Fixture() : sessions(random), handler(application, sessions, timeSource) {
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

device_platform::HttpRequest makeRequest(const Fixture& fixture) {
    device_platform::HttpRequest request;
    request.method = "POST";
    request.path = "/internal/ui/run";
    request.body = "{\"action\":\"manual-timed\"}";
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
    auto request = makeRequest(fixture);
    request.body = mute ? "{\"action\":\"mute-message\",\"id\":7}"
                        : "{\"action\":\"ack-message\",\"id\":7}";
    device_platform::HttpResponse response;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, response));
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
    auto request = makeRequest(fixture);

    request.metadata.csrfToken = "wrong-token";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, response));
    TEST_ASSERT_EQUAL_UINT16(403U, response.statusCode);

    request = makeRequest(fixture);
    request.metadata.origin = "http://other.local";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, response));
    TEST_ASSERT_EQUAL_UINT16(403U, response.statusCode);

    request = makeRequest(fixture);
    request.metadata.contentType = "text/plain";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, response));
    TEST_ASSERT_EQUAL_UINT16(415U, response.statusCode);

    request = makeRequest(fixture);
    request.metadata.cookie.reset();
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, response));
    TEST_ASSERT_EQUAL_UINT16(401U, response.statusCode);

    request = makeRequest(fixture);
    request.metadata.mutationSeq = "0";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, response));
    TEST_ASSERT_EQUAL_UINT16(400U, response.statusCode);

    request = makeRequest(fixture);
    request.body.assign(600U, 'x');
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, response));
    TEST_ASSERT_EQUAL_UINT16(413U, response.statusCode);

    request = makeRequest(fixture);
    request.method = "GET";
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, response));
    TEST_ASSERT_EQUAL_UINT16(405U, response.statusCode);
    TEST_ASSERT_FALSE(fixture.handler.handle(
        device_platform::HttpRequest{"POST", "/unrelated", "{}", {}}, dto,
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
    const auto request = makeRequest(fixture);
    device_platform::HttpResponse first;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, first));
    TEST_ASSERT_EQUAL_UINT16(200U, first.statusCode);
    TEST_ASSERT_EQUAL_STRING("{\"outcome\":\"applied\"}", first.body.c_str());
    const auto afterFirst = fixture.application.uiSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::ActiveRun),
                          static_cast<int>(afterFirst.home.mode));

    device_platform::HttpResponse replay;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, dto, replay));
    TEST_ASSERT_EQUAL_UINT16(first.statusCode, replay.statusCode);
    TEST_ASSERT_EQUAL_STRING(first.contentType.c_str(),
                             replay.contentType.c_str());
    TEST_ASSERT_EQUAL_STRING(first.body.c_str(), replay.body.c_str());
    const auto afterReplay = fixture.application.uiSnapshot();
    TEST_ASSERT_EQUAL_UINT32(afterFirst.revisions.expectedStateSequence,
                             afterReplay.revisions.expectedStateSequence);

    auto altered = request;
    altered.body = "{\"action\":\"different\"}";
    auto changedDto = dto;
    changedDto.intent = FermentationUiStopRunIntent{};
    device_platform::HttpResponse conflict;
    TEST_ASSERT_TRUE(fixture.handler.handle(altered, changedDto, conflict));
    TEST_ASSERT_EQUAL_UINT16(409U, conflict.statusCode);
    TEST_ASSERT_EQUAL_UINT32(
        afterFirst.revisions.expectedStateSequence,
        fixture.application.uiSnapshot().revisions.expectedStateSequence);
}

void test_handler_maps_stale_revision_and_invalid_program_without_mutation() {
    Fixture fixture;
    auto staleDto = startRequest(fixture.application);
    ++staleDto.expected.expectedStateSequence;
    auto request = makeRequest(fixture);
    device_platform::HttpResponse stale;
    TEST_ASSERT_TRUE(fixture.handler.handle(request, staleDto, stale));
    TEST_ASSERT_EQUAL_UINT16(409U, stale.statusCode);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::Standby),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));

    request.metadata.mutationSeq = "2";
    request.body = "{\"action\":\"unknown-program\"}";
    WebRunMutationDto unavailableProgram{
        fixture.application.uiSnapshot().revisions,
        FermentationUiStartProgramIntent{FermentationUiStartCandidate{
            "not-installed", std::nullopt, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt}}};
    device_platform::HttpResponse rejected;
    TEST_ASSERT_TRUE(
        fixture.handler.handle(request, unavailableProgram, rejected));
    TEST_ASSERT_EQUAL_UINT16(422U, rejected.statusCode);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::Standby),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

}  // namespace

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
    return UNITY_END();
}
