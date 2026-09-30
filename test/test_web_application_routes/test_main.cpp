#include <unity.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

#include "device_platform.hpp"
#include "configuration_limits.hpp"
#include "fermentation_application.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"
#include "virtual_time_source.hpp"
#include "web_application_routes.hpp"
#include "web_json_codec.hpp"

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
    if (expected.expectedUserConfigurationRevision.has_value())
        json << ",\"u\":"
             << expected.expectedUserConfigurationRevision->value();
    if (expected.expectedProgramCatalogRevision.has_value())
        json << ",\"c\":" << expected.expectedProgramCatalogRevision->value();
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
                json << "}}";
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

    request.metadata.csrfToken = "wrong-token";
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

void assertDuplicateMemberRejected(const char* body) {
    WebRunMutationDto decoded;
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        static_cast<int>(WebRunMutationDecodeStatus::Invalid),
        static_cast<int>(decodeWebRunMutation(body, decoded)), body);
}

void test_mutation_codec_rejects_conflicting_duplicate_at_root() {
    assertDuplicateMemberRejected(
        "{\"v\":1,\"r\":{\"s\":0},"
        "\"i\":{\"t\":\"reset-fault\"},"
        "\"i\":{\"t\":\"ack-message\",\"id\":7}}");
}

void test_mutation_codec_rejects_conflicting_duplicate_in_revisions() {
    assertDuplicateMemberRejected(
        "{\"v\":1,\"r\":{\"s\":0,\"s\":1},"
        "\"i\":{\"t\":\"reset-fault\"}}");
}

void test_mutation_codec_rejects_conflicting_duplicate_in_intent() {
    assertDuplicateMemberRejected(
        "{\"v\":1,\"r\":{\"s\":0},"
        "\"i\":{\"t\":\"reset-fault\","
        "\"t\":\"ack-message\",\"id\":7}}");
}

void test_mutation_codec_rejects_conflicting_duplicate_in_candidate() {
    assertDuplicateMemberRejected(
        "{\"v\":1,\"r\":{\"s\":0},"
        "\"i\":{\"t\":\"start-program\","
        "\"c\":{\"p\":\"old-program\","
        "\"p\":\"new-program\"}}}");
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
    candidate.fermentationDurationMinutes = UINT32_MAX;
    candidate.preheatEnabled = false;
    candidate.sensorMode = RunSensorMode::Product;
    candidate.completionMode = CompletionMode::CoolAndHoldUntilManualStop;
    candidate.coolingTargetCelsius = -12.5;
    candidate.holdDurationMinutes = UINT32_MAX;
    const WebRunMutationDto source{expected,
                                   FermentationUiStartProgramIntent{candidate}};
    const auto body = mutationBody(source);
    TEST_ASSERT_TRUE_MESSAGE(body.size() <= kMaximumWebRunMutationBodyBytes,
                             std::to_string(body.size()).c_str());

    device_platform::HttpRequest request;
    request.method = "POST";
    request.path = "/internal/ui/run";
    request.body = body;
    const auto fingerprint = mutationFingerprint(request);
    TEST_ASSERT_FALSE(fingerprint.empty());
    TEST_ASSERT_TRUE(fingerprint.size() <= kMaximumMutationFingerprintBytes);

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
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX,
                             *decodedCandidate.fermentationDurationMinutes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionMode::CoolAndHoldUntilManualStop),
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
    RUN_TEST(test_mutation_codec_rejects_invalid_bodies_without_partial_dto);
    RUN_TEST(test_mutation_codec_decodes_every_closed_application_intent);
    RUN_TEST(test_mutation_codec_rejects_conflicting_duplicate_at_root);
    RUN_TEST(test_mutation_codec_rejects_conflicting_duplicate_in_revisions);
    RUN_TEST(test_mutation_codec_rejects_conflicting_duplicate_in_intent);
    RUN_TEST(test_mutation_codec_rejects_conflicting_duplicate_in_candidate);
    RUN_TEST(test_maximum_product_mutation_fits_body_and_exact_replay_budget);
    RUN_TEST(test_read_only_api_projection_bounds_and_untrusted_values);
    RUN_TEST(test_read_only_api_routes_are_get_only_and_uncomposed);
    return UNITY_END();
}
