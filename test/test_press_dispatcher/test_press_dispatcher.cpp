#include <unity.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>

#include "configuration_bootstrap_store.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_text.hpp"
#include "mock_time_zone_resolver.hpp"
#include "mock_network_lifecycle.hpp"
#include "mock_secure_random_source.hpp"
#include "run_persistence_codec.hpp"
#include "run_persistence_coordinator.hpp"
#include "simulated_persistent_state_store.hpp"
#include "standard_program_catalog.hpp"
#include "state_store_key.hpp"
#include "virtual_time_source.hpp"

#include "../../main/fermentation_ui_press_dispatcher.hpp"

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    // A manual run needs the technical run limits of a commissioning-released
    // owner (O5, #172 S9) that does not exist, so the public entries refuse
    // it. This reaches the private body behind that guard to keep the
    // downstream owner path (confirm, persist, replay) covered.
    static FermentationApplicationRequestResult prepareStartManualTimedBody(
        FermentationApplication& application,
        const FermentationUiCommandContext& context,
        const ManualTimedRunValues& values) {
        return application.prepareStartManualTimedUnguarded(context, values);
    }

    static RunCommandState& runtimeState(FermentationApplication& application) {
        return *application.runtimeRunState_;
    }

    static RunPersistenceCoordinator& runPersistenceCoordinator(
        FermentationApplication& application) {
        return *application.runPersistenceCoordinator_;
    }

    static ApplicationCallSerializer::Guard enter(
        FermentationApplication& application) {
        return application.applicationCallSerializer_.enter();
    }

    // Issue #188 A: entry-predicate states for the local service area.
    static void setActiveManualRun(FermentationApplication& application,
                                   bool active) {
        TEST_ASSERT_NOT_NULL(application.runtimeRunState_.get());
        if (active) {
            application.runtimeRunState_->activeManualRun = ManualRunPlan{};
        } else {
            application.runtimeRunState_->activeManualRun.reset();
        }
    }

    static void requireService(FermentationApplication& application) {
        const auto guard = application.applicationCallSerializer_.enter();
        application.requireService(FaultCode::None);
    }

    static void setRecoveryDisposition(
        FermentationApplication& application,
        std::optional<RecoveryDisposition> disposition) {
        const auto guard = application.applicationCallSerializer_.enter();
        application.recoveryDisposition_ = disposition;
    }

    // Replaces the authentication domain within the same epoch, as the
    // runtime re-initialisation after a reset handoff does.
    static void reinitializeAuthentication(
        FermentationApplication& application) {
        const auto guard = application.applicationCallSerializer_.enter();
        application.initializeAuthentication(*application.stateStore_);
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;
using namespace fermentation::main_ui;

device_platform::StateStoreKey persistenceKey(const char* name) {
    const auto created = device_platform::StateStoreKey::create(name);
    TEST_ASSERT_TRUE(created.key.has_value());
    return *created.key;
}

device_platform::StateStoreReadResult readHead(
    device_platform_test_support::SimulatedPersistentStateStore& store) {
    return store.read(persistenceKey("rh0"), 8240U);
}

void assertSameHead(const device_platform::StateStoreReadResult& expected,
                    const device_platform::StateStoreReadResult& actual) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected.status),
                          static_cast<int>(actual.status));
    TEST_ASSERT_EQUAL_STRING(expected.value.c_str(), actual.value.c_str());
}

void installMessage(FermentationApplication& application, std::uint32_t id) {
    auto& state = FermentationApplicationTestAccess::runtimeState(application);
    state.messageCount = 1U;
    state.messageRevision = 0U;
    state.messages[0] = RuntimeMessage{};
    state.messages[0].id = id;
}

FermentationApplicationRequestResult prepareConfirmedMessage(
    FermentationApplication& application, std::uint32_t messageId, bool mute) {
    const auto snapshot = application.uiSnapshot();
    FermentationUiCommandContext context;
    context.expected = snapshot.revisions;
    context.monotonicMillis = 10U;
    const FermentationUiEnvelopePayload payload =
        mute ? FermentationUiEnvelopePayload{FermentationUiMuteMessageIntent{
                   messageId}}
             : FermentationUiEnvelopePayload{
                   FermentationUiAcknowledgeMessageIntent{messageId}};
    const auto prepared = application.prepareEnvelope(context, payload);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    const auto confirmed = application.confirmPrepared(prepared);
    TEST_ASSERT_TRUE(confirmed.request.has_value());
    return confirmed;
}

void assertCommandStatus(const FermentationUiCommandResult& result,
                         CommandStatus expected,
                         FermentationUiCommandPhase phase) {
    TEST_ASSERT_TRUE(std::holds_alternative<CommandStatus>(result.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(expected),
        static_cast<int>(std::get<CommandStatus>(result.detail)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(phase),
                          static_cast<int>(result.phase));
}

// Matches the existing test_renderer_boundary.cpp/test_local_touch_ui.cpp
// bottom-slot coordinate convention (index*80 + 20, y=220).
std::uint16_t bottomX(std::uint8_t index) {
    return static_cast<std::uint16_t>(index * 80U + 20U);
}
constexpr std::uint16_t kBottomY = 220U;

class MockHttpServerLifecycle final
    : public device_platform::IHttpServerLifecycle {
   public:
    [[nodiscard]] bool start(device_platform::IHttpRouteSink&) override {
        running_ = true;
        return true;
    }
    [[nodiscard]] bool stop() override {
        ++stopCalls;
        running_ = false;
        return true;
    }
    [[nodiscard]] bool running() const override { return running_; }

    std::size_t stopCalls{0U};

   private:
    bool running_{false};
};

class TestKdf final : public IAuthenticationKdf {
   public:
    bool derive(
        const std::string& secret, const AuthVerifier& parameters,
        std::array<std::uint8_t, kAuthenticationVerifierBytes>& out) override {
        ++calls;
        if (onDerive) {
            onDerive();
        }
        std::uint32_t state = 2166136261U;
        for (const auto byte : secret) {
            state = (state ^ static_cast<std::uint8_t>(byte)) * 16777619U;
        }
        for (const auto byte : parameters.salt) {
            state = (state ^ byte) * 16777619U;
        }
        for (auto& byte : out) {
            state = state * 1664525U + 1013904223U;
            byte = static_cast<std::uint8_t>(state >> 24U);
        }
        return true;
    }

    unsigned calls{0U};
    // Runs inside the slow KDF window (test-local hook, Issue #188 A).
    std::function<void()> onDerive;
};

// Application with the authentication stack, so the web first-time setup can
// be released from the local UI.
struct WebAccessFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    TestKdf kdf;
    FermentationApplication application;

    WebAccessFixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http,
                                           randomSource, kdf));
    }
};

struct AppFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;

    AppFixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    }
};

ManualTimedRunValues validManualTimedValues() {
    ManualTimedRunValues values;
    values.targetTemperatureCelsius = 30.0;
    values.durationMinutes = 60U;
    values.qualificationBandCelsius = 0.5;
    values.qualificationDurationMinutes = 10U;
    values.maximumTargetReachMinutes = 180U;
    return values;
}

CrossRolePlausibilityContext validOwningEvidence() {
    CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

struct OwningAppFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    FermentationApplication application;

    OwningAppFixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
        application.publishOwningRuntimeEvidence(validOwningEvidence());
    }
};

// WaitingForProduct reached through the application's own persistence
// coordinator and runtime state (the same real decisions the production
// loop would take): a preheat program is started, qualification is tracked
// and completed.  The product-insertion owner under test is not involved.
RunCheckpointTime waitingFixtureTime(const std::uint64_t monotonicMillis) {
    return {monotonicMillis, 1'700'000'000LL + static_cast<std::int64_t>(
                                                   monotonicMillis / 1'000U)};
}

ProgramDocument preheatProgramDocument() {
    auto document = FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(document.has_value());
    auto& program = document->program;
    program.preheat = true;
    program.maximumProductWaitMinutes = 30U;
    program.productSensorFailure.fallbackDelaySeconds = 30U;
    program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program.fermentationStages.front().durationMinutes = 120U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    return *document;
}

void driveToWaitingForProduct(FermentationApplication& application) {
    auto& state = FermentationApplicationTestAccess::runtimeState(application);
    auto& coordinator =
        FermentationApplicationTestAccess::runPersistenceCoordinator(
            application);
    state.processState.state = ProcessState::Standby;
    ProgramStartRequest request;
    request.envelope = {1U,
                        CommandSource::LocalDisplay,
                        100U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.runId = "persisted-run";
    request.program = preheatProgramDocument();
    request.sourceProgramRevision = RunProgramSourceRevision{1U};
    request.sensorMode = RunSensorMode::Product;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = true;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(coordinator
                             .persistCommand(state,
                                             decideProgramStart(state, request),
                                             waitingFixtureTime(100U))
                             .status));
    const auto tracking = decideProcessTransition(
        state.processState, &*state.processRunSnapshot,
        ProcessSignals{QualificationProgress::InBand, false, false},
        TransitionRequest{}, 100U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, tracking, waitingFixtureTime(100U))
                .status));
    const auto waiting = decideProcessTransition(
        state.processState, &*state.processRunSnapshot,
        ProcessSignals{QualificationProgress::Complete, false, false},
        TransitionRequest{}, 600100U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, waiting, waitingFixtureTime(600100U))
                .status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.processState.state));
}

FermentationUiCommandContext productInsertedContext(
    const FermentationApplication& application, const std::uint64_t now) {
    FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::LocalDisplay;
    context.monotonicMillis = now;
    context.expected = application.uiSnapshot().revisions;
    return context;
}

ProcessState runtimeProcessState(FermentationApplication& application) {
    return FermentationApplicationTestAccess::runtimeState(application)
        .processState.state;
}

void assertAppliedOwningResult(const WorkspacePressDispatchResult& result) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::OwningOutcome),
        static_cast<int>(result.outcome));
    TEST_ASSERT_TRUE(result.commandResult.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(result.commandResult->phase));
    TEST_ASSERT_TRUE(std::holds_alternative<RunPersistenceResultStatus>(
        result.commandResult->detail));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(std::get<RunPersistenceResultStatus>(
                              result.commandResult->detail)));
}

FermentationApplicationRequestResult prepareManualRunBody(
    OwningAppFixture& fixture) {
    FermentationUiCommandContext context;
    context.expected = fixture.application.uiSnapshot().revisions;
    context.monotonicMillis = fixture.timeSource.monotonicMillis();
    return FermentationApplicationTestAccess::prepareStartManualTimedBody(
        fixture.application, context, validManualTimedValues());
}

FermentationUiCommandResult startManualRunViaOwnerBody(
    OwningAppFixture& fixture) {
    const auto prepared = prepareManualRunBody(fixture);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    const auto confirmed = fixture.application.confirmPrepared(prepared);
    TEST_ASSERT_TRUE(confirmed.request.has_value());
    return fixture.application.applyConfirmedPrepared(*confirmed.request);
}

void assertAppliedCommandResult(const FermentationUiCommandResult& result) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(result.phase));
    TEST_ASSERT_TRUE(
        std::holds_alternative<RunPersistenceResultStatus>(result.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(std::get<RunPersistenceResultStatus>(result.detail)));
}

void test_dispatch_no_typed_payload_is_reported_as_such() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    const auto result =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(result.outcome));
    TEST_ASSERT_FALSE(result.prepareStatus.has_value());
    TEST_ASSERT_FALSE(result.confirmStatus.has_value());
    TEST_ASSERT_FALSE(result.resumeFallbackStatus.has_value());
}

// S1: choosing a program row is pure navigation. It yields no typed payload,
// so the dispatcher reaches neither an envelope nor an owning application path
// for the start or the management intent.
void test_dispatch_program_row_selection_yields_no_typed_payload() {
    AppFixture fixture;
    auto catalog = makeFactoryProgramCatalog();
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    for (const auto intent :
         {FermentationUiWorkspaceSlotAction::NavigateProgramList,
          FermentationUiWorkspaceSlotAction::NavigateProgramManagement}) {
        FermentationTouchWorkspace workspace;
        const auto home = workspace.view(snapshot, &catalog);
        const std::size_t slotIndex = home.slotActions[0] == intent ? 0U : 1U;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(intent),
                              static_cast<int>(home.slotActions[slotIndex]));
        TEST_ASSERT_TRUE(
            workspace
                .press(snapshot,
                       {device_platform::DeviceUiTargetKind::BottomSlot,
                        static_cast<std::uint8_t>(slotIndex)},
                       &catalog)
                .navigated);
        const auto press = workspace.press(
            snapshot,
            {device_platform::DeviceUiTargetKind::ContentCell, 0U, 0U, 0U},
            &catalog);
        TEST_ASSERT_TRUE(press.navigated);
        const auto result =
            dispatchWorkspacePress(fixture.application, snapshot, press, 1000U);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
            static_cast<int>(result.outcome));
        TEST_ASSERT_FALSE(result.commandResult.has_value());
        TEST_ASSERT_FALSE(result.prepareStatus.has_value());
    }
}

void test_dispatch_action_reaches_prepare_and_confirm() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    FermentationUiStopRunIntent stopIntent;
    stopIntent.option = StopOption::AbortAndTurnOff;
    press.action = FermentationUiEnvelopePayload{stopIntent};
    FermentationUiSnapshot snapshot;
    snapshot.revisions.expectedStateSequence = 0U;

    const auto result =
        dispatchWorkspacePress(fixture.application, snapshot, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::DecisionOnly),
        static_cast<int>(result.outcome));
    TEST_ASSERT_TRUE(result.prepareStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(*result.prepareStatus));
    // confirmPrepared() is genuinely reached (proving both prepare AND
    // confirm are wired): stopping needs no sensor evidence, so the existing
    // application logic confirms it and forwards that real outcome untouched.
    TEST_ASSERT_TRUE(result.confirmStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(*result.confirmStatus));
}

void test_dispatch_resume_fallback_is_forwarded_unmodified() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    // The workspace always produces confirmed=false for a fresh
    // resume-fallback press (see FermentationTouchWorkspace); the
    // dispatcher must forward it exactly, not invent confirmation.
    press.resumeFallback = FermentationUiResumeFallbackCommand{{}, false};

    const auto result =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::DecisionOnly),
        static_cast<int>(result.outcome));
    TEST_ASSERT_FALSE(result.prepareStatus.has_value());
    TEST_ASSERT_TRUE(result.resumeFallbackStatus.has_value());
    // No recovery is pending in this fresh application: the existing
    // resumeFallback() path itself rejects with NotInitialized
    // (pendingFallbackResume_ == nullptr), not a status this dispatcher
    // invents.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotInitialized),
        static_cast<int>(*result.resumeFallbackStatus));
}

void test_prepared_request_has_no_owning_mutation_before_handoff() {
    OwningAppFixture fixture;
    const auto before = fixture.application.uiSnapshot();
    FermentationUiCommandContext context;
    context.expected = before.revisions;
    context.monotonicMillis = fixture.timeSource.monotonicMillis();
    const auto prepared = prepareManualRunBody(fixture);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Prepared),
        static_cast<int>(prepared.status));
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_FALSE(prepared.request->commandEnvelope().confirmed);

    const auto after = fixture.application.uiSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::Standby),
                          static_cast<int>(after.home.mode));
    TEST_ASSERT_EQUAL_UINT32(before.revisions.expectedStateSequence,
                             after.revisions.expectedStateSequence);
}

void test_confirmed_manual_start_reaches_existing_owning_persist_path() {
    OwningAppFixture fixture;
    assertAppliedCommandResult(startManualRunViaOwnerBody(fixture));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::ActiveRun),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

void test_confirmed_start_reuses_prepared_envelope_monotonic_time() {
    OwningAppFixture fixture;
    const auto prepared = prepareManualRunBody(fixture);
    TEST_ASSERT_TRUE(prepared.request.has_value());
    const auto envelopeMonotonicMillis =
        prepared.request->commandEnvelope().monotonicMillis;
    const auto confirmed = fixture.application.confirmPrepared(prepared);
    TEST_ASSERT_TRUE(confirmed.request.has_value());

    fixture.timeSource.advanceMonotonicMillis(5U);
    const auto applied =
        fixture.application.applyConfirmedPrepared(*confirmed.request);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(applied.phase));
    TEST_ASSERT_TRUE(
        std::holds_alternative<RunPersistenceResultStatus>(applied.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(std::get<RunPersistenceResultStatus>(applied.detail)));

    const auto headRead = readHead(fixture.store);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(headRead.status));
    const auto head = decodeRunPersistenceHead(
        headRead.value, device_platform::StorageEpoch(1U));
    TEST_ASSERT_TRUE(head.has_value());
    const auto recordName = head->current.slot == 0U ? "rc0" : "rc1";
    const auto recordRead =
        fixture.store.read(persistenceKey(recordName), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(recordRead.status));
    const auto record = decodeRunPersistenceRecord(
        recordRead.value, device_platform::StorageEpoch(1U));
    TEST_ASSERT_TRUE(record.has_value());
    TEST_ASSERT_EQUAL_UINT64(envelopeMonotonicMillis,
                             record->snapshot.checkpointMonotonicMillis);
}

void test_confirmed_stop_reaches_existing_owning_persist_path() {
    OwningAppFixture fixture;
    assertAppliedCommandResult(startManualRunViaOwnerBody(fixture));

    fixture.timeSource.advanceMonotonicMillis(1U);
    FermentationUiWorkspacePress stop;
    FermentationUiStopRunIntent stopIntent;
    stopIntent.option = StopOption::AbortAndTurnOff;
    stop.action = FermentationUiEnvelopePayload{stopIntent};
    const auto stopped = dispatchWorkspacePress(
        fixture.application, fixture.application.uiSnapshot(), stop,
        fixture.timeSource.monotonicMillis());
    assertAppliedOwningResult(stopped);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::Standby),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

void test_revalidation_failure_does_not_enter_owning_path() {
    OwningAppFixture fixture;
    const auto prepared = prepareManualRunBody(fixture);
    TEST_ASSERT_TRUE(prepared.request.has_value());

    fixture.application.publishOwningRuntimeEvidence(
        CrossRolePlausibilityContext{});
    const auto rejected = fixture.application.confirmPrepared(prepared);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Unavailable),
        static_cast<int>(rejected.status));
    TEST_ASSERT_FALSE(rejected.request.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::Standby),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

void test_duplicate_confirmed_request_preserves_owner_idempotency() {
    OwningAppFixture fixture;
    const auto prepared = prepareManualRunBody(fixture);
    const auto confirmed = fixture.application.confirmPrepared(prepared);
    TEST_ASSERT_TRUE(confirmed.request.has_value());
    const auto commandId = confirmed.request->commandId();

    const auto first =
        fixture.application.applyConfirmedPrepared(*confirmed.request);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(first.phase));
    TEST_ASSERT_TRUE(
        std::holds_alternative<RunPersistenceResultStatus>(first.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(std::get<RunPersistenceResultStatus>(first.detail)));
    TEST_ASSERT_EQUAL_UINT64(commandId, confirmed.request->commandId());

    const auto replay =
        fixture.application.applyConfirmedPrepared(*confirmed.request);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::DecisionOnly),
        static_cast<int>(replay.phase));
    TEST_ASSERT_TRUE(std::holds_alternative<CommandStatus>(replay.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::AlreadyProcessed),
        static_cast<int>(std::get<CommandStatus>(replay.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::ActiveRun),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

void test_acknowledge_message_is_ram_owned_and_persistence_ineligible() {
    OwningAppFixture fixture;
    installMessage(fixture.application, 7U);
    const auto confirmed =
        prepareConfirmedMessage(fixture.application, 7U, false);
    const auto headBefore = readHead(fixture.store);

    CommandDecision decision;
    const auto decisionResult = FermentationUiCommandBridge::decidePrepared(
        FermentationApplicationTestAccess::runtimeState(fixture.application),
        *confirmed.request, std::nullopt, &decision);
    assertCommandStatus(decisionResult, CommandStatus::Proposed,
                        FermentationUiCommandPhase::DecisionOnly);
    const auto persistenceResult =
        FermentationApplicationTestAccess::runPersistenceCoordinator(
            fixture.application)
            .persistCommand(
                FermentationApplicationTestAccess::runtimeState(
                    fixture.application),
                decision,
                RunCheckpointTime{
                    confirmed.request->commandEnvelope().monotonicMillis,
                    1'700'000'000LL});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotEligible),
        static_cast<int>(persistenceResult.status));
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::runtimeState(fixture.application)
            .messages[0]
            .acknowledged);

    const auto applied =
        fixture.application.applyConfirmedPrepared(*confirmed.request);
    assertCommandStatus(applied, CommandStatus::Applied,
                        FermentationUiCommandPhase::OwningOutcome);
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::runtimeState(fixture.application)
            .messages[0]
            .acknowledged);
    assertSameHead(headBefore, readHead(fixture.store));

    const auto replay =
        fixture.application.applyConfirmedPrepared(*confirmed.request);
    assertCommandStatus(replay, CommandStatus::AlreadyProcessed,
                        FermentationUiCommandPhase::DecisionOnly);
    assertSameHead(headBefore, readHead(fixture.store));
}

void test_mute_message_is_ram_owned_and_persistence_ineligible() {
    OwningAppFixture fixture;
    installMessage(fixture.application, 8U);
    const auto confirmed =
        prepareConfirmedMessage(fixture.application, 8U, true);
    const auto headBefore = readHead(fixture.store);

    CommandDecision decision;
    const auto decisionResult = FermentationUiCommandBridge::decidePrepared(
        FermentationApplicationTestAccess::runtimeState(fixture.application),
        *confirmed.request, std::nullopt, &decision);
    assertCommandStatus(decisionResult, CommandStatus::Proposed,
                        FermentationUiCommandPhase::DecisionOnly);
    const auto persistenceResult =
        FermentationApplicationTestAccess::runPersistenceCoordinator(
            fixture.application)
            .persistCommand(
                FermentationApplicationTestAccess::runtimeState(
                    fixture.application),
                decision,
                RunCheckpointTime{
                    confirmed.request->commandEnvelope().monotonicMillis,
                    1'700'000'000LL});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotEligible),
        static_cast<int>(persistenceResult.status));
    TEST_ASSERT_FALSE(
        FermentationApplicationTestAccess::runtimeState(fixture.application)
            .messages[0]
            .acousticMuted);

    const auto applied =
        fixture.application.applyConfirmedPrepared(*confirmed.request);
    assertCommandStatus(applied, CommandStatus::Applied,
                        FermentationUiCommandPhase::OwningOutcome);
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::runtimeState(fixture.application)
            .messages[0]
            .acousticMuted);
    assertSameHead(headBefore, readHead(fixture.store));

    const auto replay =
        fixture.application.applyConfirmedPrepared(*confirmed.request);
    assertCommandStatus(replay, CommandStatus::AlreadyProcessed,
                        FermentationUiCommandPhase::DecisionOnly);
    assertSameHead(headBefore, readHead(fixture.store));
}

// S2: the message selected by a physical row press reaches the unchanged
// owning acknowledge/mute path with its canonical id.
void test_selected_message_row_reaches_the_owning_acknowledge_and_mute_path() {
    OwningAppFixture fixture;
    installMessage(fixture.application, 7U);
    const auto snapshot = fixture.application.uiSnapshot();
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(snapshot.messages.size()));

    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::Messages);
    const auto selected = workspace.press(
        snapshot,
        {device_platform::DeviceUiTargetKind::ContentCell, 0U, 0U, 0U});
    TEST_ASSERT_TRUE(selected.navigated);
    TEST_ASSERT_FALSE(selected.action.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::MessageDetail),
                          static_cast<int>(workspace.page()));

    const auto acknowledge = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 1U});
    TEST_ASSERT_TRUE(acknowledge.action.has_value());
    const auto* intent = std::get_if<FermentationUiAcknowledgeMessageIntent>(
        &*acknowledge.action);
    TEST_ASSERT_TRUE(intent != nullptr);
    TEST_ASSERT_EQUAL_UINT32(7U, intent->messageId);

    FermentationUiCommandContext context;
    context.expected = snapshot.revisions;
    context.monotonicMillis = 10U;
    const auto prepared =
        fixture.application.prepareEnvelope(context, *acknowledge.action);
    const auto confirmed = fixture.application.confirmPrepared(prepared);
    TEST_ASSERT_TRUE(confirmed.request.has_value());
    const auto applied =
        fixture.application.applyConfirmedPrepared(*confirmed.request);
    assertCommandStatus(applied, CommandStatus::Applied,
                        FermentationUiCommandPhase::OwningOutcome);
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::runtimeState(fixture.application)
            .messages[0]
            .acknowledged);

    const auto mute = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 2U});
    TEST_ASSERT_TRUE(mute.action.has_value());
    const auto* muteIntent =
        std::get_if<FermentationUiMuteMessageIntent>(&*mute.action);
    TEST_ASSERT_TRUE(muteIntent != nullptr);
    TEST_ASSERT_EQUAL_UINT32(7U, muteIntent->messageId);
}

// S3: a language row press reaches the Application's configuration path
// through the bridge and the dispatcher as an owning outcome; a stale or
// absent revision is refused and the language stays unchanged.
void test_language_row_press_reaches_the_owning_configuration_commit() {
    OwningAppFixture fixture;
    const auto current = [&fixture] {
        const auto source = fixture.application.uiPresentationSource();
        TEST_ASSERT_TRUE(source.has_value());
        return source->displayLocale.value();
    };
    TEST_ASSERT_TRUE(current() != "es");
    const auto snapshot = fixture.application.uiSnapshot();

    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderLanguage);
    const auto press = workspace.press(
        snapshot,
        {device_platform::DeviceUiTargetKind::ContentCell, 0U, 2U, 0U});
    TEST_ASSERT_TRUE(press.setDisplayLanguage.has_value());
    const auto result =
        dispatchWorkspacePress(fixture.application, snapshot, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::OwningOutcome),
        static_cast<int>(result.outcome));
    TEST_ASSERT_TRUE(result.commandResult.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(result.commandResult->phase));
    TEST_ASSERT_TRUE(std::holds_alternative<ConfigurationCommitStatus>(
        result.commandResult->detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(
            std::get<ConfigurationCommitStatus>(result.commandResult->detail)));
    TEST_ASSERT_EQUAL_STRING("es", current().c_str());

    // The same (now stale) snapshot revision is refused; nothing changes.
    const auto stale = workspace.press(
        snapshot,
        {device_platform::DeviceUiTargetKind::ContentCell, 0U, 1U, 0U});
    const auto refused =
        dispatchWorkspacePress(fixture.application, snapshot, stale, 1001U);
    TEST_ASSERT_TRUE(refused.commandResult.has_value());
    TEST_ASSERT_TRUE(refused.commandResult->category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_EQUAL_STRING("es", current().c_str());

    // An absent revision is refused fail-closed as well.
    auto undecidable = fixture.application.uiSnapshot();
    undecidable.revisions.expectedUserConfigurationRevision.reset();
    const auto absent = workspace.press(
        undecidable,
        {device_platform::DeviceUiTargetKind::ContentCell, 0U, 0U, 0U});
    const auto absentResult =
        dispatchWorkspacePress(fixture.application, undecidable, absent, 1002U);
    TEST_ASSERT_TRUE(absentResult.commandResult.has_value());
    TEST_ASSERT_TRUE(absentResult.commandResult->category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_EQUAL_STRING("es", current().c_str());
}

// S10: a device name commit from the keyboard reaches the Application's
// settings path through the bridge and the dispatcher as an owning outcome;
// while a run is active the Application refuses it (the UI row is only the
// display of that gate) and the touch adapter shows the refusal.
void test_device_name_commit_reaches_the_owner_and_an_active_run_refuses_it() {
    OwningAppFixture fixture;
    const auto name = [&fixture] {
        const auto source = fixture.application.uiPresentationSource();
        TEST_ASSERT_TRUE(source.has_value());
        return source->deviceName;
    };
    const auto typeNeu = [](FermentationTouchWorkspace& workspace,
                            const FermentationUiSnapshot& snapshot) {
        // lower case: n = row 1 column 3, e = row 0 column 4, u = row 2 col 0
        for (const auto& cell : {std::pair<std::uint8_t, std::uint8_t>{1U, 3U},
                                 {0U, 4U},
                                 {2U, 0U}}) {
            TEST_ASSERT_TRUE(
                workspace
                    .press(snapshot,
                           {device_platform::DeviceUiTargetKind::ContentCell,
                            0U, cell.first, cell.second})
                    .navigated);
        }
    };
    FermentationTouchWorkspace workspace;
    workspace.adoptDeviceName(name());
    workspace.setPage(FermentationUiPage::TextEdit);
    auto snapshot = fixture.application.uiSnapshot();
    typeNeu(workspace, snapshot);
    const auto commit = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 3U});
    TEST_ASSERT_TRUE(commit.setDeviceName.has_value());
    const auto result =
        dispatchWorkspacePress(fixture.application, snapshot, commit, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::OwningOutcome),
        static_cast<int>(result.outcome));
    TEST_ASSERT_TRUE(result.commandResult.has_value());
    TEST_ASSERT_TRUE(std::holds_alternative<ConfigurationCommitStatus>(
        result.commandResult->detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(
            std::get<ConfigurationCommitStatus>(result.commandResult->detail)));
    TEST_ASSERT_EQUAL_STRING("neu", name().c_str());

    // With a run active the owner refuses the change; nothing changes.
    assertAppliedCommandResult(startManualRunViaOwnerBody(fixture));
    snapshot = fixture.application.uiSnapshot();
    TEST_ASSERT_FALSE(snapshot.home.activeRunId.empty());
    FermentationTouchWorkspace second;
    second.adoptDeviceName(name());
    second.setPage(FermentationUiPage::Settings);
    TEST_ASSERT_FALSE(second.view(snapshot).settings->deviceNameEditable);

    // A UI that still shows the keyboard (stale) is refused by the owner and
    // the settings page names the refusal.
    second.setPage(FermentationUiPage::TextEdit);
    typeNeu(second, snapshot);
    const auto packs = makeFermentationUiTextPacks();
    const auto touched = processWorkspaceTouch(
        fixture.application, second, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, bottomX(3), kBottomY,
        /*freshPressEdge=*/true, fixture.timeSource.monotonicMillis());
    TEST_ASSERT_TRUE(touched.dispatch.commandResult.has_value());
    TEST_ASSERT_TRUE(touched.dispatch.commandResult->category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_EQUAL_STRING("neu", name().c_str());
    TEST_ASSERT_TRUE(second.page() == FermentationUiPage::Settings);
    TEST_ASSERT_TRUE(second.view(snapshot).blockedReason ==
                     std::optional<device_platform::TextKey>{
                         fermentationTextKey("device-name-change-failed")});
}

// S10 review B1: through the real touch adapter and the S6 owner, an accepted
// save makes the editor clean; a save the owner refuses (stale catalog
// revision) keeps the candidate, the dirty state and the discard protection.
void test_program_save_outcome_decides_whether_the_editor_is_clean() {
    OwningAppFixture fixture;
    const auto packs = makeFermentationUiTextPacks();
    const auto source = fixture.application.uiPresentationSource();
    TEST_ASSERT_TRUE(source.has_value());
    const auto& catalog = source->programCatalog;
    // A user program: copy the first programs list entry via the owner first.
    FermentationUiProgramEditRequest copy;
    copy.operation = FermentationUiProgramEditOperation::Copy;
    copy.programId = catalog.programs.front().program.id;
    copy.confirmed = true;
    auto snapshot = fixture.application.uiSnapshot();
    const auto copied = fixture.application.applyProgramEdit(
        copy, snapshot.revisions.expectedProgramCatalogRevision,
        snapshot.revisions.expectedUserConfigurationRevision);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(copied.commit));
    const auto after = fixture.application.uiPresentationSource();
    const auto& userProgram = after->programCatalog.programs.back();

    FermentationTouchWorkspace workspace;
    TEST_ASSERT_TRUE(
        workspace.selectProgram(userProgram.program.id, after->programCatalog));
    workspace.setProgramEditOperation(FermentationUiProgramEditOperation::Edit);
    workspace.setPage(FermentationUiPage::ProgramEdit);
    auto edited = userProgram;
    edited.program.notes = "neu";
    workspace.setProgramEditCandidate(edited);
    workspace.setProgramEditDirty(true);

    const auto touchSave = [&](const FermentationUiSnapshot& shown) {
        return processWorkspaceTouch(
            fixture.application, workspace, shown, packs,
            device_platform::LocaleId{"en"}, &after->programCatalog,
            device_platform::DeviceUiNetworkStatus::Unavailable, {},
            /*contactHeld=*/true, bottomX(3), kBottomY,
            /*freshPressEdge=*/true, fixture.timeSource.monotonicMillis());
    };

    // Stale: the UI shows an older catalog revision than the owner holds.
    snapshot = fixture.application.uiSnapshot();
    auto stale = snapshot;
    stale.revisions.expectedProgramCatalogRevision = ProgramCatalogRevision{
        snapshot.revisions.expectedProgramCatalogRevision->value() + 1U};
    const auto refused = touchSave(stale);
    TEST_ASSERT_TRUE(refused.dispatch.commandResult.has_value());
    TEST_ASSERT_TRUE(refused.dispatch.commandResult->category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    auto view = workspace.view(snapshot, &after->programCatalog);
    TEST_ASSERT_TRUE(view.route.exitRequirement ==
                     device_platform::PageExitRequirement::ConfirmDiscard);
    TEST_ASSERT_TRUE(view.slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::DiscardProgramEdit);
    TEST_ASSERT_EQUAL_STRING("", fixture.application.uiPresentationSource()
                                     ->programCatalog.programs.back()
                                     .program.notes.c_str());
    const auto blockedBack = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Back, 0U},
        &after->programCatalog);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiInteractionOutcome::Blocked),
        static_cast<int>(blockedBack.interaction.outcome));

    // Release the touch, then the same save with the current revision.
    static_cast<void>(processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, &after->programCatalog,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/false, bottomX(3), kBottomY,
        /*freshPressEdge=*/false, fixture.timeSource.monotonicMillis()));
    const auto accepted = touchSave(snapshot);
    TEST_ASSERT_TRUE(accepted.dispatch.commandResult.has_value());
    TEST_ASSERT_TRUE(accepted.dispatch.commandResult->category ==
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_EQUAL_STRING("neu", fixture.application.uiPresentationSource()
                                        ->programCatalog.programs.back()
                                        .program.notes.c_str());
    view = workspace.view(
        fixture.application.uiSnapshot(),
        &fixture.application.uiPresentationSource()->programCatalog);
    TEST_ASSERT_TRUE(view.route.exitRequirement ==
                     device_platform::PageExitRequirement::None);
}

// Review B1: a refused language change is shown on the language page, keeps
// the language, is replaced by the next accepted change and discarded when
// the page is left. Driven through the real touch adapter.
void test_refused_language_change_is_visible_and_cleared_by_a_success() {
    OwningAppFixture fixture;
    const auto current = [&fixture] {
        return fixture.application.uiPresentationSource()
            ->displayLocale.value();
    };
    const auto packs = makeFermentationUiTextPacks();
    const device_platform::ClockViewInput clock{1'700'000'000LL, {}};
    const auto touch = [&](FermentationTouchWorkspace& workspace,
                           const FermentationUiSnapshot& snapshot,
                           std::uint16_t y) {
        return processWorkspaceTouch(
            fixture.application, workspace, snapshot, packs,
            device_platform::LocaleId{"en"}, nullptr,
            device_platform::DeviceUiNetworkStatus::Connected, clock, true,
            100U, y, true, 1000U);
    };
    const auto failureText = [&packs](const char* locale) {
        return device_platform::resolveText(
                   packs, device_platform::LocaleId{locale},
                   fermentationTextKey("language-change-failed"))
            .value;
    };
    const auto screenHas = [&](FermentationTouchWorkspace& workspace,
                               const FermentationUiSnapshot& snapshot,
                               const char* locale, const std::string& text) {
        const auto screen = makeRepresentativeScreen(
            snapshot, workspace, packs, device_platform::LocaleId{locale});
        for (const auto& command : screen.commands)
            if (command.text == text) return true;
        return false;
    };

    // 1.-2. Open the page with a snapshot, then change the configuration so
    // that snapshot's revision is stale.
    const auto stale = fixture.application.uiSnapshot();
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderLanguage);
    TEST_ASSERT_TRUE(current() != "es");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(
            fixture.application
                .applyDisplayLanguage(
                    "es", stale.revisions.expectedUserConfigurationRevision)
                .commit));
    TEST_ASSERT_FALSE(workspace.view(stale).blockedReason.has_value());

    // 3.-5. Row 0 ("de") with the stale revision through the touch adapter.
    const auto refused = touch(workspace, stale, 70U);
    TEST_ASSERT_TRUE(refused.dispatch.commandResult.has_value());
    TEST_ASSERT_TRUE(refused.dispatch.commandResult->category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_EQUAL_STRING("es", current().c_str());

    // 6. A localized, visible failure message on the language page.
    TEST_ASSERT_TRUE(workspace.view(stale).blockedReason.has_value());
    for (const char* locale : {"en", "de", "es"}) {
        const auto text = failureText(locale);
        TEST_ASSERT_TRUE(!text.empty());
        TEST_ASSERT_TRUE(text.find("fermentation") == std::string::npos);
        TEST_ASSERT_TRUE(screenHas(workspace, stale, locale, text));
    }

    // 7. A following accepted change takes over and removes the message.
    const auto fresh = fixture.application.uiSnapshot();
    const auto accepted = touch(workspace, fresh, 110U);
    TEST_ASSERT_TRUE(accepted.dispatch.commandResult.has_value());
    TEST_ASSERT_TRUE(accepted.dispatch.commandResult->category ==
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_EQUAL_STRING("en", current().c_str());
    TEST_ASSERT_FALSE(workspace.view(fresh).blockedReason.has_value());
    TEST_ASSERT_FALSE(screenHas(workspace, fresh, "en", failureText("en")));

    // Leaving the page discards a transient failure.
    static_cast<void>(touch(workspace, stale, 70U));
    TEST_ASSERT_TRUE(workspace.view(stale).blockedReason.has_value());
    workspace.setPage(FermentationUiPage::Home);
    workspace.setPage(FermentationUiPage::HeaderLanguage);
    TEST_ASSERT_FALSE(workspace.view(stale).blockedReason.has_value());
}

void test_command_status_projection_keeps_decisions_only() {
    const auto proposed =
        FermentationUiCommandBridge::fromCommandStatus(CommandStatus::Proposed);
    assertCommandStatus(proposed, CommandStatus::Proposed,
                        FermentationUiCommandPhase::DecisionOnly);
    const auto replay = FermentationUiCommandBridge::fromCommandStatus(
        CommandStatus::AlreadyProcessed);
    assertCommandStatus(replay, CommandStatus::AlreadyProcessed,
                        FermentationUiCommandPhase::DecisionOnly);

    const auto owning =
        FermentationUiCommandBridge::fromOwningCommandApplyStatus(
            CommandStatus::Applied);
    assertCommandStatus(owning, CommandStatus::Applied,
                        FermentationUiCommandPhase::OwningOutcome);
}

void test_product_inserted_without_runtime_context_is_context_missing() {
    // An application that has not begun owns no runtime/persistence context.
    FermentationApplication application;
    FermentationUiCommandContext context;
    context.monotonicMillis = 700'000U;
    const auto result = application.confirmProductInserted(context);
    assertCommandStatus(result, CommandStatus::ContextMissing,
                        FermentationUiCommandPhase::DecisionOnly);
    TEST_ASSERT_TRUE(result.category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
}

void test_product_inserted_waiting_for_product_reaches_target_and_persists() {
    OwningAppFixture fixture;
    driveToWaitingForProduct(fixture.application);
    const auto headBefore = readHead(fixture.store);

    const auto result = fixture.application.confirmProductInserted(
        productInsertedContext(fixture.application, 700'000U));
    TEST_ASSERT_TRUE(
        std::holds_alternative<RunPersistenceResultStatus>(result.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(std::get<RunPersistenceResultStatus>(result.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(result.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(runtimeProcessState(fixture.application)));
    const auto headAfter = readHead(fixture.store);
    TEST_ASSERT_TRUE(headBefore.value != headAfter.value);
}

void test_product_inserted_in_wrong_state_is_rejected_without_change() {
    OwningAppFixture fixture;
    const auto headBefore = readHead(fixture.store);
    const auto before = runtimeProcessState(fixture.application);

    const auto result = fixture.application.confirmProductInserted(
        productInsertedContext(fixture.application, 700'000U));
    TEST_ASSERT_TRUE(std::holds_alternative<DecisionStatus>(result.detail));
    TEST_ASSERT_TRUE(std::get<DecisionStatus>(result.detail) !=
                     DecisionStatus::Proposed);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(result.category));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(before),
        static_cast<int>(runtimeProcessState(fixture.application)));
    assertSameHead(headBefore, readHead(fixture.store));
}

void test_product_inserted_stale_sequence_is_rejected_without_change() {
    OwningAppFixture fixture;
    driveToWaitingForProduct(fixture.application);
    const auto headBefore = readHead(fixture.store);
    auto context = productInsertedContext(fixture.application, 700'000U);
    context.expected.expectedStateSequence += 1U;

    const auto result = fixture.application.confirmProductInserted(context);
    assertCommandStatus(result, CommandStatus::StaleState,
                        FermentationUiCommandPhase::DecisionOnly);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::WaitingForProduct),
        static_cast<int>(runtimeProcessState(fixture.application)));
    assertSameHead(headBefore, readHead(fixture.store));
}

void test_product_inserted_persistence_failure_is_fail_closed_and_retryable() {
    OwningAppFixture fixture;
    driveToWaitingForProduct(fixture.application);
    const auto headBefore = readHead(fixture.store);

    fixture.store.setNextWriteFault(
        device_platform_test_support::SimulatedPersistentStateStore::
            WriteFault::FailBeforeBegin);
    const auto failed = fixture.application.confirmProductInserted(
        productInsertedContext(fixture.application, 700'000U));
    TEST_ASSERT_TRUE(failed.category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_TRUE(
        std::holds_alternative<RunPersistenceResultStatus>(failed.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(std::get<RunPersistenceResultStatus>(failed.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::WaitingForProduct),
        static_cast<int>(runtimeProcessState(fixture.application)));
    assertSameHead(headBefore, readHead(fixture.store));

    const auto retried = fixture.application.confirmProductInserted(
        productInsertedContext(fixture.application, 700'000U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(std::get<RunPersistenceResultStatus>(retried.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(runtimeProcessState(fixture.application)));
}

void test_product_inserted_repeated_press_is_rejected() {
    OwningAppFixture fixture;
    driveToWaitingForProduct(fixture.application);
    const auto firstContext =
        productInsertedContext(fixture.application, 700'000U);
    const auto first = fixture.application.confirmProductInserted(firstContext);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(std::get<RunPersistenceResultStatus>(first.detail)));
    const auto headAfterFirst = readHead(fixture.store);

    // The same (now stale) press arrives again.
    const auto second =
        fixture.application.confirmProductInserted(firstContext);
    assertCommandStatus(second, CommandStatus::StaleState,
                        FermentationUiCommandPhase::DecisionOnly);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(runtimeProcessState(fixture.application)));
    assertSameHead(headAfterFirst, readHead(fixture.store));
}

void test_dispatch_transition_action_reaches_the_owning_application() {
    OwningAppFixture fixture;
    driveToWaitingForProduct(fixture.application);
    FermentationUiWorkspacePress press;
    press.transitionAction = FermentationUiProductInsertedConfirmedIntent{};

    const auto result = dispatchWorkspacePress(
        fixture.application, fixture.application.uiSnapshot(), press, 700'000U);
    assertAppliedOwningResult(result);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(runtimeProcessState(fixture.application)));
}

void test_dispatch_transition_action_without_context_is_decision_only() {
    FermentationApplication application;
    FermentationUiWorkspacePress press;
    press.transitionAction = FermentationUiProductInsertedConfirmedIntent{};

    const auto result = dispatchWorkspacePress(application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::DecisionOnly),
        static_cast<int>(result.outcome));
    TEST_ASSERT_TRUE(result.commandResult.has_value());
    assertCommandStatus(*result.commandResult, CommandStatus::ContextMissing,
                        FermentationUiCommandPhase::DecisionOnly);
}

// ---- S6: ProgramCatalog mutation owner -----------------------------------

using ProgramEditOp = FermentationUiProgramEditOperation;

ProgramCatalog activeCatalog(const FermentationApplication& application) {
    const auto source = application.uiPresentationSource();
    TEST_ASSERT_TRUE(source.has_value());
    return source->programCatalog;
}

const ProgramDocument* findProgram(const ProgramCatalog& catalog,
                                   const std::string& id) {
    for (const auto& document : catalog.programs)
        if (document.program.id == id) return &document;
    return nullptr;
}

std::size_t userProgramCount(const ProgramCatalog& catalog) {
    std::size_t count = 0U;
    for (const auto& document : catalog.programs)
        if (!document.program.factoryCatalogEntry) ++count;
    return count;
}

ApplicationConfigurationChangeResult applyEdit(
    FermentationApplication& application, const ProgramEditOp operation,
    const std::string& programId,
    std::optional<ProgramDocument> candidate = std::nullopt,
    std::optional<std::string> name = std::nullopt, bool confirmed = true) {
    const auto revisions = application.uiSnapshot().revisions;
    return application.applyProgramEdit(
        FermentationUiProgramEditRequest{operation, programId,
                                         std::move(candidate), std::move(name),
                                         confirmed},
        revisions.expectedProgramCatalogRevision,
        revisions.expectedUserConfigurationRevision);
}

void assertActivated(const ApplicationConfigurationChangeResult& result) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(result.preview));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(result.commit));
}

void assertRejectedPreview(const ApplicationConfigurationChangeResult& result,
                           const ConfigurationPreviewStatus expected) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected),
                          static_cast<int>(result.preview));
}

void activateProgramRun(FermentationApplication& application,
                        const ProgramDocument& document) {
    auto active = ActiveRun::start(document, ProgramSourceKind::FactoryCatalog,
                                   RunProgramSourceRevision{1U});
    TEST_ASSERT_TRUE(active.has_value());
    auto& state = FermentationApplicationTestAccess::runtimeState(application);
    state.processState.state = ProcessState::Fermenting;
    state.activeProgramRun = std::move(*active);
}

void test_program_copy_and_new_create_user_programs() {
    OwningAppFixture fixture;
    const auto before = activeCatalog(fixture.application);
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(userProgramCount(before)));

    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-mild"));
    auto after = activeCatalog(fixture.application);
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(userProgramCount(after)));
    const auto* copy = findProgram(after, "user-00");
    TEST_ASSERT_TRUE(copy != nullptr);
    TEST_ASSERT_EQUAL_STRING("Joghurt mild copy", copy->program.name.c_str());
    TEST_ASSERT_TRUE(copy->program.userDeletable);

    assertActivated(applyEdit(fixture.application, ProgramEditOp::New, ""));
    after = activeCatalog(fixture.application);
    TEST_ASSERT_EQUAL_UINT32(
        2U, static_cast<std::uint32_t>(userProgramCount(after)));
    const auto* created = findProgram(after, "user-01");
    TEST_ASSERT_TRUE(created != nullptr);
    // Without a candidate or name the new program is "New program", built
    // from the water-kefir template with cleared run parameters: selectable
    // and deletable, not startable.
    TEST_ASSERT_EQUAL_STRING("New program", created->program.name.c_str());
    TEST_ASSERT_FALSE(created->program.factoryCatalogEntry);
    TEST_ASSERT_TRUE(created->program.userDeletable);
    TEST_ASSERT_FALSE(created->program.fermentationStages.front()
                          .targetTemperatureCelsius.has_value());
}

void test_program_delete_removes_user_program_but_never_a_factory_program() {
    OwningAppFixture fixture;
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-mild"));
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Delete, "user-00"));
    TEST_ASSERT_TRUE(
        findProgram(activeCatalog(fixture.application), "user-00") == nullptr);

    const auto refused =
        applyEdit(fixture.application, ProgramEditOp::Delete, "yogurt-mild");
    assertRejectedPreview(refused,
                          ConfigurationPreviewStatus::InvalidCandidate);
    TEST_ASSERT_TRUE(findProgram(activeCatalog(fixture.application),
                                 "yogurt-mild") != nullptr);
}

void test_program_uninstall_needs_confirmation_and_reset_restores_factory() {
    OwningAppFixture fixture;
    const auto unconfirmed =
        applyEdit(fixture.application, ProgramEditOp::Uninstall, "yogurt-mild",
                  std::nullopt, std::nullopt, false);
    assertRejectedPreview(unconfirmed,
                          ConfigurationPreviewStatus::InvalidCandidate);
    TEST_ASSERT_TRUE(
        findProgram(activeCatalog(fixture.application), "yogurt-mild")
            ->program.installed);

    assertActivated(applyEdit(fixture.application, ProgramEditOp::Uninstall,
                              "yogurt-mild"));
    TEST_ASSERT_FALSE(
        findProgram(activeCatalog(fixture.application), "yogurt-mild")
            ->program.installed);

    // Edit a second standard program, then reset it to the factory values
    // (canonical wire value 6, StandardProgramReset).
    auto candidate =
        *findProgram(activeCatalog(fixture.application), "yogurt-firm");
    candidate.program.name = "Joghurt geaendert";
    assertActivated(applyEdit(fixture.application, ProgramEditOp::Edit,
                              "yogurt-firm", candidate));
    TEST_ASSERT_EQUAL_STRING(
        "Joghurt geaendert",
        findProgram(activeCatalog(fixture.application), "yogurt-firm")
            ->program.name.c_str());
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Reset, "yogurt-firm"));
    TEST_ASSERT_EQUAL_STRING(
        "Joghurt stichfest",
        findProgram(activeCatalog(fixture.application), "yogurt-firm")
            ->program.name.c_str());
}

void test_program_edit_of_the_active_program_is_blocked_before_preview() {
    OwningAppFixture fixture;
    // The run uses the runnable water-kefir values; the catalog entry with
    // the same id is the "program in use".
    activateProgramRun(fixture.application, preheatProgramDocument());
    const auto activeSnapshotName =
        storedProgram(
            FermentationApplicationTestAccess::runtimeState(fixture.application)
                .activeProgramRun->snapshot()
                .source)
            ->program.name;

    const auto blocked =
        applyEdit(fixture.application, ProgramEditOp::Uninstall, "water-kefir");
    assertRejectedPreview(blocked, ConfigurationPreviewStatus::NotAllowed);
    TEST_ASSERT_TRUE(
        findProgram(activeCatalog(fixture.application), "water-kefir")
            ->program.installed);

    // No preview slot was taken, and the running snapshot stays untouched
    // while another program is mutated.
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-firm"));
    TEST_ASSERT_EQUAL_STRING(
        activeSnapshotName.c_str(),
        storedProgram(
            FermentationApplicationTestAccess::runtimeState(fixture.application)
                .activeProgramRun->snapshot()
                .source)
            ->program.name.c_str());
}

void test_program_edit_and_reset_of_the_active_program_are_blocked() {
    OwningAppFixture fixture;
    activateProgramRun(fixture.application, preheatProgramDocument());
    const auto before =
        *findProgram(activeCatalog(fixture.application), "water-kefir");

    auto candidate = before;
    candidate.program.name = "Wasserkefir geaendert";
    assertRejectedPreview(applyEdit(fixture.application, ProgramEditOp::Edit,
                                    "water-kefir", candidate),
                          ConfigurationPreviewStatus::NotAllowed);
    assertRejectedPreview(
        applyEdit(fixture.application, ProgramEditOp::Reset, "water-kefir"),
        ConfigurationPreviewStatus::NotAllowed);
    assertRejectedPreview(
        applyEdit(fixture.application, ProgramEditOp::Delete, "water-kefir"),
        ConfigurationPreviewStatus::NotAllowed);
    TEST_ASSERT_EQUAL_STRING(
        before.program.name.c_str(),
        findProgram(activeCatalog(fixture.application), "water-kefir")
            ->program.name.c_str());

    // Copy only reads the source and New addresses no entry; neither is
    // blocked, and no preview slot stayed taken by the rejected calls.
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Copy, "water-kefir"));
    assertActivated(applyEdit(fixture.application, ProgramEditOp::New, ""));
    TEST_ASSERT_EQUAL_STRING(
        before.program.name.c_str(),
        storedProgram(
            FermentationApplicationTestAccess::runtimeState(fixture.application)
                .activeProgramRun->snapshot()
                .source)
            ->program.name.c_str());
}

void test_program_edit_with_stale_catalog_revision_is_rejected() {
    OwningAppFixture fixture;
    const auto revisions = fixture.application.uiSnapshot().revisions;
    TEST_ASSERT_TRUE(revisions.expectedProgramCatalogRevision.has_value());
    const FermentationUiProgramEditRequest request{
        ProgramEditOp::Copy, "yogurt-mild", std::nullopt, std::nullopt, true};
    const auto stale = fixture.application.applyProgramEdit(
        request,
        ProgramCatalogRevision{
            revisions.expectedProgramCatalogRevision->value() + 1U},
        revisions.expectedUserConfigurationRevision);
    assertRejectedPreview(stale, ConfigurationPreviewStatus::StateChanged);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(userProgramCount(
                                     activeCatalog(fixture.application))));

    // The failure released the preview slot.
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-mild"));
}

// The user-configuration revision the user saw must reach the confirmation
// check; the current runtime value is never substituted.
void test_program_edit_from_a_stale_user_configuration_snapshot_is_rejected() {
    OwningAppFixture fixture;
    const auto seen = fixture.application.uiSnapshot().revisions;
    TEST_ASSERT_TRUE(seen.expectedUserConfigurationRevision.has_value());
    TEST_ASSERT_TRUE(seen.expectedProgramCatalogRevision.has_value());

    // A user-configuration change that leaves the program catalog untouched.
    const auto language = fixture.application.applyDisplayLanguage(
        "es", seen.expectedUserConfigurationRevision);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(language.commit));
    const auto current = fixture.application.uiSnapshot().revisions;
    TEST_ASSERT_TRUE(*current.expectedUserConfigurationRevision !=
                     *seen.expectedUserConfigurationRevision);
    TEST_ASSERT_TRUE(*current.expectedProgramCatalogRevision ==
                     *seen.expectedProgramCatalogRevision);

    const FermentationUiProgramEditRequest request{
        ProgramEditOp::Copy, "yogurt-mild", std::nullopt, std::nullopt, true};
    const auto stale = fixture.application.applyProgramEdit(
        request, seen.expectedProgramCatalogRevision,
        seen.expectedUserConfigurationRevision);
    TEST_ASSERT_TRUE(stale.commit != ConfigurationCommitStatus::Activated);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(userProgramCount(
                                     activeCatalog(fixture.application))));

    // The same press through the dispatcher with the old snapshot is rejected
    // as well.
    FermentationUiWorkspacePress press;
    press.programEdit = request;
    FermentationUiSnapshot staleSnapshot;
    staleSnapshot.revisions = seen;
    const auto dispatched = dispatchWorkspacePress(fixture.application,
                                                   staleSnapshot, press, 1000U);
    TEST_ASSERT_TRUE(dispatched.commandResult.has_value());
    TEST_ASSERT_TRUE(dispatched.commandResult->category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(userProgramCount(
                                     activeCatalog(fixture.application))));

    // No preview slot stayed taken: the current snapshot still works.
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-mild"));
}

void test_program_edit_without_expected_revisions_is_fail_closed() {
    OwningAppFixture fixture;
    const auto revisions = fixture.application.uiSnapshot().revisions;
    const FermentationUiProgramEditRequest request{
        ProgramEditOp::Copy, "yogurt-mild", std::nullopt, std::nullopt, true};

    const auto noCatalog = fixture.application.applyProgramEdit(
        request, std::nullopt, revisions.expectedUserConfigurationRevision);
    assertRejectedPreview(noCatalog, ConfigurationPreviewStatus::StateChanged);
    const auto noUser = fixture.application.applyProgramEdit(
        request, revisions.expectedProgramCatalogRevision, std::nullopt);
    assertRejectedPreview(noUser, ConfigurationPreviewStatus::StateChanged);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(userProgramCount(
                                     activeCatalog(fixture.application))));

    // Current snapshot: the existing success path stays green.
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-mild"));
}

// Waits until the worker is blocked on the shared application gate held by
// the caller, then releases it and expects the worker to finish.
template <typename Call>
void assertCallWaitsForTheApplicationGate(FermentationApplication& application,
                                          Call call) {
    std::mutex synchronization;
    std::condition_variable changed;
    bool workerStarted = false;
    bool workerDone = false;

    std::thread worker;
    {
        auto held = FermentationApplicationTestAccess::enter(application);
        worker = std::thread([&] {
            {
                std::lock_guard<std::mutex> lock(synchronization);
                workerStarted = true;
            }
            changed.notify_one();
            call();
            {
                std::lock_guard<std::mutex> lock(synchronization);
                workerDone = true;
            }
            changed.notify_one();
        });
        {
            std::unique_lock<std::mutex> lock(synchronization);
            TEST_ASSERT_TRUE(changed.wait_for(lock, std::chrono::seconds(1),
                                              [&] { return workerStarted; }));
            // Give the worker a chance to (wrongly) pass the gate.
            changed.wait_for(lock, std::chrono::milliseconds(100),
                             [&] { return workerDone; });
            TEST_ASSERT_FALSE(workerDone);
        }
    }
    {
        std::unique_lock<std::mutex> lock(synchronization);
        TEST_ASSERT_TRUE(changed.wait_for(lock, std::chrono::seconds(2),
                                          [&] { return workerDone; }));
    }
    worker.join();
}

// D5: like every public Application entry, the new owners wait for the shared
// application gate.
void test_confirm_product_inserted_waits_for_the_application_gate() {
    OwningAppFixture fixture;
    driveToWaitingForProduct(fixture.application);
    const auto context = productInsertedContext(fixture.application, 700'000U);
    FermentationUiCommandResult result;
    assertCallWaitsForTheApplicationGate(fixture.application, [&] {
        result = fixture.application.confirmProductInserted(context);
    });
    TEST_ASSERT_TRUE(
        std::holds_alternative<RunPersistenceResultStatus>(result.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(std::get<RunPersistenceResultStatus>(result.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(runtimeProcessState(fixture.application)));
}

void test_apply_program_edit_waits_for_the_application_gate() {
    OwningAppFixture fixture;
    const auto revisions = fixture.application.uiSnapshot().revisions;
    ApplicationConfigurationChangeResult result;
    assertCallWaitsForTheApplicationGate(fixture.application, [&] {
        result = fixture.application.applyProgramEdit(
            FermentationUiProgramEditRequest{ProgramEditOp::Copy, "yogurt-mild",
                                             std::nullopt, std::nullopt, true},
            revisions.expectedProgramCatalogRevision,
            revisions.expectedUserConfigurationRevision);
    });
    assertActivated(result);
    TEST_ASSERT_EQUAL_UINT32(1U, static_cast<std::uint32_t>(userProgramCount(
                                     activeCatalog(fixture.application))));
}

void test_program_capacity_is_reported_and_recoverable() {
    OwningAppFixture fixture;
    for (std::size_t index = 0U; index < 12U; ++index) {
        assertActivated(
            applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-mild"));
    }
    TEST_ASSERT_EQUAL_UINT32(12U, static_cast<std::uint32_t>(userProgramCount(
                                      activeCatalog(fixture.application))));
    const auto full =
        applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-mild");
    assertRejectedPreview(full, ConfigurationPreviewStatus::InvalidCandidate);
    TEST_ASSERT_EQUAL_UINT32(12U, static_cast<std::uint32_t>(userProgramCount(
                                      activeCatalog(fixture.application))));

    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Delete, "user-00"));
    assertActivated(applyEdit(fixture.application, ProgramEditOp::New, ""));
}

void test_program_edit_without_run_state_or_service_is_fail_closed() {
    FermentationApplication application;
    const auto result = application.applyProgramEdit(
        FermentationUiProgramEditRequest{ProgramEditOp::Copy, "yogurt-mild",
                                         std::nullopt, std::nullopt, true},
        ProgramCatalogRevision{1U}, UserConfigurationRevision{1U});
    assertRejectedPreview(
        result, ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable);
}

void test_program_edit_persists_across_a_restart() {
    OwningAppFixture fixture;
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Copy, "yogurt-mild"));
    assertActivated(
        applyEdit(fixture.application, ProgramEditOp::Uninstall, "milk-kefir"));

    FermentationApplication restarted;
    TEST_ASSERT_TRUE(restarted.begin(fixture.platform, fixture.store,
                                     fixture.timeZoneResolver,
                                     fixture.timeSource));
    const auto catalog = activeCatalog(restarted);
    TEST_ASSERT_TRUE(findProgram(catalog, "user-00") != nullptr);
    TEST_ASSERT_FALSE(findProgram(catalog, "milk-kefir")->program.installed);
}

void test_dispatch_program_edit_reaches_the_owning_application() {
    OwningAppFixture fixture;
    FermentationUiWorkspacePress press;
    press.programEdit = FermentationUiProgramEditRequest{
        ProgramEditOp::Copy, "yogurt-mild", std::nullopt, std::nullopt, true};

    const auto result = dispatchWorkspacePress(
        fixture.application, fixture.application.uiSnapshot(), press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::OwningOutcome),
        static_cast<int>(result.outcome));
    TEST_ASSERT_TRUE(result.commandResult.has_value());
    TEST_ASSERT_TRUE(std::holds_alternative<ConfigurationCommitStatus>(
        result.commandResult->detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(
            std::get<ConfigurationCommitStatus>(result.commandResult->detail)));
    TEST_ASSERT_TRUE(
        findProgram(activeCatalog(fixture.application), "user-00") != nullptr);

    // A snapshot without a catalog revision cannot authorize a change.
    const auto unauthorized =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_TRUE(unauthorized.commandResult.has_value());
    TEST_ASSERT_TRUE(unauthorized.commandResult->category !=
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_EQUAL_UINT32(1U, static_cast<std::uint32_t>(userProgramCount(
                                     activeCatalog(fixture.application))));
}

void test_dispatch_network_touch_actions_use_existing_application_bridge() {
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

    auto snapshot = application.uiSnapshot();
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderNetwork);
    const auto selected = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 1U});
    TEST_ASSERT_TRUE(selected.applyNetworkMode.has_value());
    const auto selectedResult =
        dispatchWorkspacePress(application, snapshot, selected, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::OwningOutcome),
        static_cast<int>(selectedResult.outcome));
    TEST_ASSERT_TRUE(selectedResult.commandResult.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(std::get<NetworkConfigurationStatus>(
                              selectedResult.commandResult->detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkMode::AP_ONLY),
        static_cast<int>(application.networkMode()));

    snapshot = application.uiSnapshot();
    workspace.setPage(FermentationUiPage::HeaderNetwork);
    const auto changedToHome = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 2U});
    TEST_ASSERT_TRUE(changedToHome.applyNetworkMode.has_value());
    const auto homeResult =
        dispatchWorkspacePress(application, snapshot, changedToHome, 1001U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(std::get<NetworkConfigurationStatus>(
                              homeResult.commandResult->detail)));

    snapshot = application.uiSnapshot();
    workspace.setPage(FermentationUiPage::HeaderNetwork);
    const auto reconfigure = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 3U});
    TEST_ASSERT_TRUE(reconfigure.beginHomeWifiReconfiguration.has_value());
    const auto reconfigureResult =
        dispatchWorkspacePress(application, snapshot, reconfigure, 1002U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(std::get<NetworkConfigurationStatus>(
                              reconfigureResult.commandResult->detail)));
}

void test_web_access_open_runs_through_bridge_to_the_application_owner() {
    WebAccessFixture fixture;
    auto snapshot = fixture.application.uiSnapshot();
    TEST_ASSERT_TRUE(snapshot.webAccess == FermentationWebAccessState::Closed);

    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderWebAccess);
    const auto press = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 1U});
    TEST_ASSERT_TRUE(press.openWebProvisioningWindow.has_value());

    const auto opened =
        dispatchWorkspacePress(fixture.application, snapshot, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::OwningOutcome),
        static_cast<int>(opened.outcome));
    TEST_ASSERT_TRUE(opened.commandResult.has_value());
    TEST_ASSERT_TRUE(opened.commandResult->category ==
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_TRUE(
        std::get<FermentationUiDetailStatus>(opened.commandResult->detail) ==
        FermentationUiDetailStatus::WebProvisioningWindowOpened);

    // The owner state is projected back into the UI snapshot and the slot.
    snapshot = fixture.application.uiSnapshot();
    TEST_ASSERT_TRUE(snapshot.webAccess ==
                     FermentationWebAccessState::WindowOpen);
    TEST_ASSERT_FALSE(workspace.view(snapshot).bottomSlots[1].enabled);
    const auto blocked = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 1U});
    TEST_ASSERT_FALSE(blocked.openWebProvisioningWindow.has_value());

    // Dispatching the same command again is decided by the owner: it neither
    // reopens nor extends the window.
    const auto again =
        dispatchWorkspacePress(fixture.application, snapshot, press, 1001U);
    TEST_ASSERT_TRUE(again.commandResult->category ==
                     device_platform::DeviceUiCommandOutcomeCategory::Rejected);
    TEST_ASSERT_TRUE(
        std::get<FermentationUiDetailStatus>(again.commandResult->detail) ==
        FermentationUiDetailStatus::WebProvisioningWindowNotOpened);

    // The window expires in the Application, the UI follows.
    fixture.timeSource.advanceMonotonicMillis(kWebProvisioningWindowMs);
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().webAccess ==
                     FermentationWebAccessState::Closed);
}

void test_web_access_provisioning_success_is_projected_as_not_applicable() {
    WebAccessFixture fixture;
    auto snapshot = fixture.application.uiSnapshot();
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderWebAccess);
    const auto press = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 1U});
    TEST_ASSERT_TRUE(
        dispatchWorkspacePress(fixture.application, snapshot, press, 1000U)
            .commandResult->category ==
        device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_TRUE(fixture.application.provisionWebAccess(
                         WebProvisionMode::Disable, "", "1234") ==
                     WebProvisionStatus::Provisioned);
    snapshot = fixture.application.uiSnapshot();
    TEST_ASSERT_TRUE(snapshot.webAccess ==
                     FermentationWebAccessState::NotApplicable);
    TEST_ASSERT_FALSE(workspace.view(snapshot).bottomSlots[1].enabled);
}

void test_web_access_not_allowed_state_is_not_bypassed_by_the_ui() {
    // No authentication stack: web access is not applicable. Even a forged
    // command is decided by the Application owner.
    AppFixture fixture;
    auto snapshot = fixture.application.uiSnapshot();
    TEST_ASSERT_TRUE(snapshot.webAccess ==
                     FermentationWebAccessState::NotApplicable);
    FermentationTouchWorkspace workspace;
    workspace.setPage(FermentationUiPage::HeaderWebAccess);
    TEST_ASSERT_FALSE(workspace.view(snapshot).bottomSlots[1].enabled);
    const auto blocked = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 1U});
    TEST_ASSERT_FALSE(blocked.openWebProvisioningWindow.has_value());

    FermentationUiWorkspacePress forged;
    forged.openWebProvisioningWindow =
        FermentationUiOpenWebProvisioningWindowCommand{};
    const auto result =
        dispatchWorkspacePress(fixture.application, snapshot, forged, 1000U);
    TEST_ASSERT_TRUE(result.commandResult->category ==
                     device_platform::DeviceUiCommandOutcomeCategory::Rejected);
    TEST_ASSERT_TRUE(
        std::get<FermentationUiDetailStatus>(result.commandResult->detail) ==
        FermentationUiDetailStatus::WebProvisioningWindowNotOpened);
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().webAccess ==
                     FermentationWebAccessState::NotApplicable);
}

void test_process_touch_without_contact_yields_no_target() {
    AppFixture fixture;
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    const auto packs = makeFermentationUiTextPacks();

    const auto tick = processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/false, 0U, 0U, /*freshPressEdge=*/false, 1000U);

    TEST_ASSERT_FALSE(tick.pressedTarget.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(tick.dispatch.outcome));
}

void test_process_touch_held_without_fresh_edge_does_not_navigate() {
    AppFixture fixture;
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    const auto packs = makeFermentationUiTextPacks();

    const auto tick = processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, bottomX(1), kBottomY,
        /*freshPressEdge=*/false, 1000U);

    TEST_ASSERT_TRUE(tick.pressedTarget.has_value());
    TEST_ASSERT_EQUAL_UINT8(1U, tick.pressedTarget->slotIndex);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(tick.dispatch.outcome));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Home),
                          static_cast<int>(workspace.page()));
}

void test_process_touch_fresh_edge_on_valid_slot_navigates() {
    AppFixture fixture;
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    const auto packs = makeFermentationUiTextPacks();

    // Home/Standby slot 1 is "programs" -> NavigateProgramList (pure
    // navigation, no typed payload).
    const auto tick = processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, bottomX(1), kBottomY,
        /*freshPressEdge=*/true, 1000U);

    TEST_ASSERT_TRUE(tick.pressedTarget.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(tick.dispatch.outcome));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::ProgramList),
                          static_cast<int>(workspace.page()));
}

void test_process_touch_from_home_reaches_network_page_and_application_owner() {
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

    auto snapshot = application.uiSnapshot();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationHomeMode::Standby),
                          static_cast<int>(snapshot.home.mode));
    FermentationTouchWorkspace workspace;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Home),
                          static_cast<int>(workspace.page()));
    const auto packs = makeFermentationUiTextPacks();

    const auto headerTouch = processWorkspaceTouch(
        application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, 240U, 12U, /*freshPressEdge=*/true, 1000U);
    TEST_ASSERT_TRUE(headerTouch.pressedTarget.has_value());
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DeviceUiTargetKind::HeaderNetwork),
        static_cast<int>(headerTouch.pressedTarget->kind));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::HeaderNetwork),
                          static_cast<int>(workspace.page()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
        static_cast<int>(headerTouch.dispatch.outcome));

    snapshot = application.uiSnapshot();
    const auto actionTouch = processWorkspaceTouch(
        application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, bottomX(1U), kBottomY,
        /*freshPressEdge=*/true, 1001U);
    TEST_ASSERT_TRUE(actionTouch.pressedTarget.has_value());
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DeviceUiTargetKind::BottomSlot),
        static_cast<int>(actionTouch.pressedTarget->kind));
    TEST_ASSERT_EQUAL_UINT8(1U, actionTouch.pressedTarget->slotIndex);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::OwningOutcome),
        static_cast<int>(actionTouch.dispatch.outcome));
    TEST_ASSERT_TRUE(actionTouch.dispatch.commandResult.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(actionTouch.dispatch.commandResult->phase));
    TEST_ASSERT_TRUE(std::holds_alternative<NetworkConfigurationStatus>(
        actionTouch.dispatch.commandResult->detail));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(NetworkConfigurationStatus::Applied),
                          static_cast<int>(std::get<NetworkConfigurationStatus>(
                              actionTouch.dispatch.commandResult->detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NetworkMode::AP_ONLY),
        static_cast<int>(application.networkMode()));
}

void test_process_touch_fresh_edge_off_target_does_not_navigate() {
    AppFixture fixture;
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    snapshot.home.mode = FermentationHomeMode::Standby;
    const auto packs = makeFermentationUiTextPacks();

    const auto tick = processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {},
        /*contactHeld=*/true, 20U, 20U, /*freshPressEdge=*/true, 1000U);

    TEST_ASSERT_FALSE(tick.pressedTarget.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FermentationUiPage::Home),
                          static_cast<int>(workspace.page()));
}

// S9 / O5: the technical run limits of a manual run have no owner (#34/#35),
// so no surface can start a manual run or a cooling plan: the physical touch
// sequence produces no payload, and a manual payload that reaches the
// dispatcher anyway is refused by the Application (Unavailable) before any
// identity is used.
void test_manual_start_has_no_path_without_an_owner_of_the_run_limits() {
    OwningAppFixture fixture;
    FermentationTouchWorkspace workspace;
    const auto snapshot = fixture.application.uiSnapshot();
    const auto packs = makeFermentationUiTextPacks();
    workspace.setPage(FermentationUiPage::ManualTimed);
    workspace.setManualTimedValues(validManualTimedValues());
    const auto view = workspace.view(snapshot);
    TEST_ASSERT_FALSE(view.bottomSlots[2].enabled);
    TEST_ASSERT_TRUE(view.blockedReason ==
                     fermentationTextKey("manual-parameters-not-released"));
    for (int attempt = 0; attempt < 2; ++attempt) {
        const auto start = processWorkspaceTouch(
            fixture.application, workspace, snapshot, packs,
            device_platform::LocaleId{"en"}, nullptr,
            device_platform::DeviceUiNetworkStatus::Unavailable, {},
            /*contactHeld=*/attempt == 0, bottomX(2), kBottomY,
            /*freshPressEdge=*/attempt == 0,
            fixture.timeSource.monotonicMillis());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(WorkspacePressDispatchOutcome::NoTypedPayload),
            static_cast<int>(start.dispatch.outcome));
        TEST_ASSERT_FALSE(start.dispatch.prepareStatus.has_value());
    }

    // A payload built outside the UI reaches the Application and is refused.
    FermentationUiWorkspacePress forced;
    forced.action = FermentationUiEnvelopePayload{
        FermentationUiStartManualTimedIntent{validManualTimedValues()}};
    const auto dispatched =
        dispatchWorkspacePress(fixture.application, snapshot, forced,
                               fixture.timeSource.monotonicMillis());
    TEST_ASSERT_TRUE(dispatched.prepareStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Unavailable),
        static_cast<int>(*dispatched.prepareStatus));
    TEST_ASSERT_FALSE(dispatched.commandResult.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::Standby),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

}  // namespace

// The native test target does not compile the ESP-IDF main component, so
// the two app-owned main/ sources this test exercises are included here
// directly (the same way test_renderer_boundary.cpp already does for
// fermentation_ui_renderer.cpp). Unlike that narrower test, this one also
// includes fermentation_application.hpp, so PlatformIO's library dependency
// finder already links the full fermentation_app library (including
// fermentation_touch_workspace.cpp) - those production sources are
// therefore deliberately NOT inline-included again here, to avoid duplicate
// symbol definitions.
#include "../../main/fermentation_ui_press_dispatcher.cpp"
#include "../../main/fermentation_ui_renderer.cpp"

// --- Issue #19: local factory reset through the real touch adapter ---------

constexpr std::uint32_t kFactoryResetHoldMs = 1500U;  // test value only

WorkspaceTouchTickResult factoryResetTouch(
    OwningAppFixture& fixture, FermentationTouchWorkspace& workspace,
    bool contactHeld, std::uint8_t slot, bool fresh, std::uint64_t nowMs) {
    const auto packs = makeFermentationUiTextPacks();
    const auto snapshot = fixture.application.uiSnapshot();
    return processWorkspaceTouch(
        fixture.application, workspace, snapshot, packs,
        device_platform::LocaleId{"en"}, nullptr,
        device_platform::DeviceUiNetworkStatus::Unavailable, {}, contactHeld,
        bottomX(slot), kBottomY, fresh, nowMs);
}

void test_forgot_pin_entry_runs_the_whole_flow_through_touch() {
    OwningAppFixture fixture;
    fixture.application.setFactoryResetHoldMillis(kFactoryResetHoldMs);
    FermentationTouchWorkspace workspace;
    // The PIN page is reachable while the service area is locked.
    workspace.setPage(FermentationUiPage::Service);
    auto snapshot = fixture.application.uiSnapshot();
    TEST_ASSERT_FALSE(snapshot.service.available);
    TEST_ASSERT_TRUE(workspace.view(snapshot).bottomSlots[1].enabled);
    workspace.setPage(FermentationUiPage::Pin);
    snapshot = fixture.application.uiSnapshot();
    TEST_ASSERT_TRUE(snapshot.factoryReset.available);
    TEST_ASSERT_TRUE(workspace.view(snapshot).slotActions[3] ==
                     FermentationUiWorkspaceSlotAction::FactoryResetBegin);

    // "PIN forgotten?" starts the flow without any PIN entry.
    auto step = factoryResetTouch(fixture, workspace, true, 3U, true, 1000U);
    TEST_ASSERT_TRUE(step.dispatch.commandResult.has_value());
    TEST_ASSERT_TRUE(step.dispatch.commandResult->category ==
                     device_platform::DeviceUiCommandOutcomeCategory::Accepted);
    TEST_ASSERT_TRUE(workspace.page() == FermentationUiPage::FactoryReset);
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().factoryReset.stage ==
                     FactoryResetStage::Warning);
    static_cast<void>(
        factoryResetTouch(fixture, workspace, false, 3U, false, 1100U));
    // Warning -> Confirm -> Hold, one deliberate tap each.
    static_cast<void>(
        factoryResetTouch(fixture, workspace, true, 2U, true, 1200U));
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().factoryReset.stage ==
                     FactoryResetStage::Confirm);
    static_cast<void>(
        factoryResetTouch(fixture, workspace, false, 2U, false, 1300U));
    static_cast<void>(
        factoryResetTouch(fixture, workspace, true, 2U, true, 1400U));
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().factoryReset.stage ==
                     FactoryResetStage::Hold);
    static_cast<void>(
        factoryResetTouch(fixture, workspace, false, 2U, false, 1500U));

    // The long press counts only on the hold slot and only if continuous.
    const std::uint64_t t0 = 5000U;
    static_cast<void>(
        factoryResetTouch(fixture, workspace, true, 1U, true, t0));
    static_cast<void>(factoryResetTouch(fixture, workspace, true, 1U, false,
                                        t0 + kFactoryResetHoldMs - 1U));
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().factoryReset.stage ==
                     FactoryResetStage::Hold);
    // Sliding off the hold slot releases the long press.
    static_cast<void>(factoryResetTouch(fixture, workspace, true, 3U, false,
                                        t0 + kFactoryResetHoldMs));
    static_cast<void>(factoryResetTouch(fixture, workspace, true, 1U, false,
                                        t0 + kFactoryResetHoldMs + 10U));
    static_cast<void>(factoryResetTouch(
        fixture, workspace, true, 1U, false,
        t0 + kFactoryResetHoldMs + 10U + kFactoryResetHoldMs - 1U));
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().factoryReset.stage ==
                     FactoryResetStage::Hold);
    // Releasing the contact also restarts the count.
    static_cast<void>(factoryResetTouch(fixture, workspace, false, 1U, false,
                                        t0 + 3U * kFactoryResetHoldMs));
    static_cast<void>(factoryResetTouch(fixture, workspace, true, 1U, false,
                                        t0 + 4U * kFactoryResetHoldMs));
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().factoryReset.stage ==
                     FactoryResetStage::Hold);
    static_cast<void>(
        factoryResetTouch(fixture, workspace, true, 1U, false,
                          t0 + 4U * kFactoryResetHoldMs + kFactoryResetHoldMs));
    const auto done = fixture.application.uiSnapshot();
    TEST_ASSERT_TRUE(done.factoryReset.stage == FactoryResetStage::Finished);
    TEST_ASSERT_TRUE(done.factoryReset.outcome ==
                     FactoryResetOutcome::Completed);
    // Acknowledging the result returns to the home page and closes the flow.
    static_cast<void>(factoryResetTouch(fixture, workspace, false, 0U, false,
                                        t0 + 6U * kFactoryResetHoldMs));
    static_cast<void>(factoryResetTouch(fixture, workspace, true, 0U, true,
                                        t0 + 7U * kFactoryResetHoldMs));
    TEST_ASSERT_TRUE(workspace.page() == FermentationUiPage::Home);
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().factoryReset.stage ==
                     FactoryResetStage::Idle);
}

void test_safe_boot_entry_and_every_exit_end_the_flow_without_a_reset() {
    OwningAppFixture fixture;
    fixture.application.setFactoryResetHoldMillis(kFactoryResetHoldMs);
    FermentationTouchWorkspace workspace;
    auto snapshot = fixture.application.uiSnapshot();
    // SAFE_BOOT home: its own local entry, independent of the service area.
    snapshot.home.mode = FermentationHomeMode::Recovery;
    snapshot.home.processState = ProcessState::SafeBoot;
    snapshot.service.available = false;
    const auto home = workspace.view(snapshot);
    TEST_ASSERT_TRUE(home.slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::FactoryResetBegin);
    TEST_ASSERT_TRUE(home.bottomSlots[0].enabled);
    // The reset is offered, so it is not listed as unavailable.
    for (const auto capability : home.unavailableCapabilities) {
        TEST_ASSERT_TRUE(
            capability !=
            FermentationUiSafeBootCapability::PersistentFactoryReset);
    }
    const auto press = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::BottomSlot, 0U});
    TEST_ASSERT_TRUE(press.factoryReset.has_value());
    const auto dispatched =
        dispatchWorkspacePress(fixture.application, snapshot, press, 1000U);
    TEST_ASSERT_TRUE(dispatched.commandResult.has_value());
    TEST_ASSERT_TRUE(fixture.application.factoryResetView(0U).stage ==
                     FactoryResetStage::Warning);

    // The generic Back target leaves the page and cancels the armed flow.
    snapshot = fixture.application.uiSnapshot();
    const auto back = workspace.press(
        snapshot, {device_platform::DeviceUiTargetKind::Back, 0U});
    TEST_ASSERT_TRUE(back.factoryReset.has_value());
    static_cast<void>(
        dispatchWorkspacePress(fixture.application, snapshot, back, 1100U));
    TEST_ASSERT_TRUE(fixture.application.factoryResetView(0U).stage ==
                     FactoryResetStage::Idle);

    // Without the owner's hold parameter the entry is offered nowhere.
    fixture.application.setFactoryResetHoldMillis(std::nullopt);
    snapshot = fixture.application.uiSnapshot();
    snapshot.home.mode = FermentationHomeMode::Recovery;
    snapshot.home.processState = ProcessState::SafeBoot;
    TEST_ASSERT_FALSE(workspace.view(snapshot).bottomSlots[0].enabled);
    workspace.setPage(FermentationUiPage::Pin);
    TEST_ASSERT_FALSE(workspace.view(snapshot).bottomSlots[3].enabled);
}

// Issue #19 review B1: the regression runs on the REAL loop decision
// (`touchLoopAction`, the one function the firmware loop branches on). A
// release that never reaches the application used to leave the old start time
// armed, so a later short contact could trigger the reset.
void test_interrupted_hold_never_survives_a_pause_in_the_real_loop_decision() {
    WebAccessFixture fixture;
    fixture.application.setFactoryResetHoldMillis(5000U);
    FermentationTouchWorkspace workspace;
    const auto packs = makeFermentationUiTextPacks();
    const auto epochOf = [&fixture] {
        const auto scan = ConfigurationBootstrapStore(fixture.store).scan();
        TEST_ASSERT_TRUE(scan.loaded.has_value());
        return scan.loaded->record.storageEpoch.value();
    };

    // One iteration of the UI loop with the given touch sample.
    const auto loopStep = [&](bool contactHeld, std::uint8_t slot, bool fresh,
                              std::uint64_t nowMs) {
        const auto snapshot = fixture.application.uiSnapshot();
        switch (touchLoopAction(contactHeld, snapshot)) {
            case TouchLoopAction::ProcessContact:
                static_cast<void>(processWorkspaceTouch(
                    fixture.application, workspace, snapshot, packs,
                    device_platform::LocaleId{"en"}, nullptr,
                    device_platform::DeviceUiNetworkStatus::Unavailable, {},
                    true, bottomX(slot), kBottomY, fresh, nowMs));
                break;
            case TouchLoopAction::ReleaseFactoryResetHold:
                releaseFactoryResetHold(fixture.application, nowMs);
                break;
            case TouchLoopAction::None:
                break;
        }
    };
    const auto stage = [&fixture] {
        return fixture.application.uiSnapshot().factoryReset.stage;
    };

    // Reach the hold stage through the real taps.
    workspace.setPage(FermentationUiPage::Pin);
    loopStep(true, 3U, true, 100U);
    loopStep(false, 0U, false, 150U);
    loopStep(true, 2U, true, 200U);
    loopStep(false, 0U, false, 250U);
    loopStep(true, 2U, true, 300U);
    loopStep(false, 0U, false, 350U);
    TEST_ASSERT_TRUE(stage() == FactoryResetStage::Hold);
    const auto epochBefore = epochOf();

    // First, partial hold (3 s of 5 s), then the release.
    loopStep(true, 1U, true, 10000U);
    loopStep(true, 1U, false, 11500U);
    loopStep(true, 1U, false, 13000U);
    loopStep(false, 0U, false, 13100U);
    // Long pause (idle iterations), then a short new contact.
    loopStep(false, 0U, false, 60000U);
    loopStep(true, 1U, true, 70000U);
    loopStep(true, 1U, false, 70100U);
    TEST_ASSERT_TRUE(stage() == FactoryResetStage::Hold);
    TEST_ASSERT_EQUAL_UINT64(epochBefore, epochOf());
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(fixture.network.stopCallCount()));
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(fixture.http.stopCalls));

    // Sliding off the hold target and cancelling never resets either.
    loopStep(true, 3U, false, 70200U);
    loopStep(true, 1U, false, 70300U);
    loopStep(true, 1U, false, 75200U);
    TEST_ASSERT_TRUE(stage() == FactoryResetStage::Hold);
    TEST_ASSERT_EQUAL_UINT64(epochBefore, epochOf());

    // Only 5000 ms of uninterrupted contact on the hold target trigger it.
    loopStep(false, 0U, false, 80000U);
    loopStep(true, 1U, true, 90000U);
    loopStep(true, 1U, false, 94999U);
    TEST_ASSERT_TRUE(stage() == FactoryResetStage::Hold);
    TEST_ASSERT_EQUAL_UINT64(epochBefore, epochOf());
    loopStep(true, 1U, false, 95000U);
    TEST_ASSERT_TRUE(stage() == FactoryResetStage::Finished);
    TEST_ASSERT_EQUAL_UINT64(epochBefore + 1U, epochOf());
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(fixture.network.stopCallCount()));
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(fixture.http.stopCalls));
}

// Issue #19 S1 (SIM-19-S1-06): the restricted no-runtime home page gains only
// the reset entry on slot 0; every other slot keeps its existing contract.
void test_restricted_no_runtime_home_adds_only_the_reset_entry() {
    OwningAppFixture fixture;
    fixture.application.setFactoryResetHoldMillis(5000U);
    FermentationTouchWorkspace workspace;
    auto base = fixture.application.uiSnapshot();
    base.home.mode = FermentationHomeMode::Restricted;
    base.home.processState = ProcessState::Boot;
    base.factoryReset.recoveryEntry = false;
    base.factoryReset.available = true;
    auto admitted = base;
    admitted.factoryReset.recoveryEntry = true;

    const auto before = workspace.view(base);
    const auto after = workspace.view(admitted);
    TEST_ASSERT_TRUE(after.slotActions[0] ==
                     FermentationUiWorkspaceSlotAction::FactoryResetBegin);
    TEST_ASSERT_TRUE(after.bottomSlots[0].enabled);
    TEST_ASSERT_TRUE(before.slotActions[0] !=
                     FermentationUiWorkspaceSlotAction::FactoryResetBegin);
    for (std::size_t slot = 1U; slot < 4U; ++slot) {
        TEST_ASSERT_TRUE(after.slotActions[slot] == before.slotActions[slot]);
        TEST_ASSERT_EQUAL(before.bottomSlots[slot].enabled,
                          after.bottomSlots[slot].enabled);
    }
    // The existing contract: status stays readable, the program list is
    // reachable at Boot, the service slot stays disabled.
    TEST_ASSERT_TRUE(after.bottomSlots[1].enabled);
    TEST_ASSERT_TRUE(after.bottomSlots[2].enabled);
    TEST_ASSERT_FALSE(after.bottomSlots[3].enabled);
    // Without the recovery-core admission nothing is added.
    admitted.factoryReset.recoveryEntry = false;
    TEST_ASSERT_TRUE(workspace.view(admitted).slotActions[0] !=
                     FermentationUiWorkspaceSlotAction::FactoryResetBegin);
}

// ---- Issue #188 A: local Service-PIN and local service lease (C1) ---------

constexpr std::uint64_t kLocalServiceInactivityMs = 10U * 60U * 1000U;

// Authentication stack plus a restartable Application on the same store.
struct LocalServiceFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    TestKdf kdf;
    std::optional<FermentationApplication> application;

    LocalServiceFixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        start();
    }

    void start() {
        application.emplace();
        TEST_ASSERT_TRUE(application->begin(platform, store, timeZoneResolver,
                                            timeSource, network, http,
                                            randomSource, kdf));
    }

    void restart() {
        application.reset();
        store.restart();
        start();
    }

    void provision() {
        TEST_ASSERT_TRUE(application->openWebProvisioningWindow());
        TEST_ASSERT_TRUE(application->provisionWebAccess(
                             WebProvisionMode::Disable, "", "1234") ==
                         WebProvisionStatus::Provisioned);
    }

    [[nodiscard]] FermentationApplication& app() { return *application; }
    [[nodiscard]] bool serviceAvailable() {
        return application->uiSnapshot().service.available;
    }
};

void assertPinStatus(LocalServicePinStatus expected,
                     const LocalServicePinResult& result) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected),
                          static_cast<int>(result.status));
}

void test_local_service_pin_grants_the_lease_only_after_a_correct_pin() {
    LocalServiceFixture fixture;
    fixture.provision();
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
    assertPinStatus(LocalServicePinStatus::Invalid,
                    fixture.app().verifyLocalServicePin("9999"));
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
    TEST_ASSERT_TRUE(fixture.serviceAvailable());
}

void test_local_service_lockout_holds_during_the_wait_and_after_restart() {
    LocalServiceFixture fixture;
    fixture.provision();
    LocalServicePinResult last;
    for (int attempt = 0; attempt < 3; ++attempt) {
        last = fixture.app().verifyLocalServicePin("9999");
    }
    assertPinStatus(LocalServicePinStatus::LockedOut, last);
    TEST_ASSERT_TRUE(last.retryAfterMs > 0U);
    // The correct PIN does not open the service area during the lockout.
    const auto blocked = fixture.app().verifyLocalServicePin("1234");
    assertPinStatus(LocalServicePinStatus::LockedOut, blocked);
    TEST_ASSERT_FALSE(fixture.serviceAvailable());

    // The persisted lockout survives a restart; no lease comes back.
    fixture.restart();
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
    assertPinStatus(LocalServicePinStatus::LockedOut,
                    fixture.app().verifyLocalServicePin("1234"));
    fixture.timeSource.advanceMonotonicMillis(31U * 1000U);
    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
    TEST_ASSERT_TRUE(fixture.serviceAvailable());
}

void test_local_service_pin_without_provisioning_runs_no_kdf() {
    LocalServiceFixture fixture;
    const auto callsBefore = fixture.kdf.calls;
    assertPinStatus(LocalServicePinStatus::NotProvisioned,
                    fixture.app().verifyLocalServicePin("1234"));
    TEST_ASSERT_EQUAL_UINT(callsBefore, fixture.kdf.calls);
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
}

void test_local_service_pin_is_refused_outside_validated_standby() {
    LocalServiceFixture fixture;
    fixture.provision();
    const auto callsBefore = fixture.kdf.calls;
    FermentationApplicationTestAccess::setActiveManualRun(fixture.app(), true);
    assertPinStatus(LocalServicePinStatus::NotAllowedInState,
                    fixture.app().verifyLocalServicePin("1234"));
    FermentationApplicationTestAccess::setActiveManualRun(fixture.app(), false);

    for (const auto state : {ProcessState::Fault, ProcessState::SafeBoot,
                             ProcessState::RecoveryEvaluation}) {
        auto& runtime =
            FermentationApplicationTestAccess::runtimeState(fixture.app());
        const auto saved = runtime.processState.state;
        runtime.processState.state = state;
        assertPinStatus(LocalServicePinStatus::NotAllowedInState,
                        fixture.app().verifyLocalServicePin("1234"));
        runtime.processState.state = saved;
    }
    FermentationApplicationTestAccess::setRecoveryDisposition(
        fixture.app(), RecoveryDisposition::WaitingForTrustedTime);
    assertPinStatus(LocalServicePinStatus::NotAllowedInState,
                    fixture.app().verifyLocalServicePin("1234"));
    FermentationApplicationTestAccess::setRecoveryDisposition(fixture.app(),
                                                              std::nullopt);
    // Every refusal was decided before the KDF and any auth write.
    TEST_ASSERT_EQUAL_UINT(callsBefore, fixture.kdf.calls);
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
}

void test_local_service_pin_refuses_a_run_started_during_the_check() {
    LocalServiceFixture fixture;
    fixture.provision();
    fixture.kdf.onDerive = [&fixture] {
        FermentationApplicationTestAccess::setActiveManualRun(fixture.app(),
                                                              true);
    };
    assertPinStatus(LocalServicePinStatus::NotAllowedInState,
                    fixture.app().verifyLocalServicePin("1234"));
    fixture.kdf.onDerive = nullptr;
    FermentationApplicationTestAccess::setActiveManualRun(fixture.app(), false);
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
}

void test_local_service_lease_ends_after_ten_minutes_without_activity() {
    LocalServiceFixture fixture;
    fixture.provision();
    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
    fixture.timeSource.advanceMonotonicMillis(kLocalServiceInactivityMs - 1U);
    TEST_ASSERT_TRUE(fixture.serviceAvailable());
    // Relevant activity restarts the inactivity window.
    fixture.app().noteLocalServiceActivity();
    fixture.timeSource.advanceMonotonicMillis(kLocalServiceInactivityMs - 1U);
    TEST_ASSERT_TRUE(fixture.serviceAvailable());
    fixture.timeSource.advanceMonotonicMillis(1U);
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
    // Activity after the expiry never resurrects the lease.
    fixture.app().noteLocalServiceActivity();
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
}

void test_local_service_sign_out_ends_the_lease() {
    LocalServiceFixture fixture;
    fixture.provision();
    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
    fixture.app().endLocalServiceSession();
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
}

void test_local_service_lease_ends_for_good_when_standby_is_left() {
    LocalServiceFixture fixture;
    fixture.provision();
    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
    FermentationApplicationTestAccess::setActiveManualRun(fixture.app(), true);
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
    fixture.app().update();
    FermentationApplicationTestAccess::setActiveManualRun(fixture.app(), false);
    // Back in STANDBY the PIN is needed again.
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
}

void test_local_service_lease_ends_on_service_required_and_recovery() {
    LocalServiceFixture fixture;
    fixture.provision();
    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
    FermentationApplicationTestAccess::setRecoveryDisposition(
        fixture.app(), RecoveryDisposition::RecoveryRejectedOrFailClosed);
    // The predicate alone already keeps the lease from acting ...
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
    // ... and the next update() ends it for good.
    fixture.app().update();
    FermentationApplicationTestAccess::setRecoveryDisposition(fixture.app(),
                                                              std::nullopt);
    TEST_ASSERT_FALSE(fixture.serviceAvailable());

    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
    const auto callsBefore = fixture.kdf.calls;
    // ServiceRequired while the run state still reports Standby.
    FermentationApplicationTestAccess::requireService(fixture.app());
    TEST_ASSERT_TRUE(
        FermentationApplicationTestAccess::runtimeState(fixture.app())
            .processState.state == ProcessState::Standby);
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
    assertPinStatus(LocalServicePinStatus::NotAllowedInState,
                    fixture.app().verifyLocalServicePin("1234"));
    TEST_ASSERT_EQUAL_UINT(callsBefore, fixture.kdf.calls);
}

void test_local_service_lease_ends_on_authentication_reinitialisation() {
    LocalServiceFixture fixture;
    fixture.provision();
    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
    FermentationApplicationTestAccess::reinitializeAuthentication(
        fixture.app());
    TEST_ASSERT_FALSE(fixture.serviceAvailable());
    // The re-initialised domain still knows the PIN.
    assertPinStatus(LocalServicePinStatus::Authorized,
                    fixture.app().verifyLocalServicePin("1234"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_restricted_no_runtime_home_adds_only_the_reset_entry);
    RUN_TEST(
        test_interrupted_hold_never_survives_a_pause_in_the_real_loop_decision);
    RUN_TEST(test_forgot_pin_entry_runs_the_whole_flow_through_touch);
    RUN_TEST(test_safe_boot_entry_and_every_exit_end_the_flow_without_a_reset);
    RUN_TEST(test_dispatch_no_typed_payload_is_reported_as_such);
    RUN_TEST(test_dispatch_program_row_selection_yields_no_typed_payload);
    RUN_TEST(test_dispatch_action_reaches_prepare_and_confirm);
    RUN_TEST(test_dispatch_resume_fallback_is_forwarded_unmodified);
    RUN_TEST(test_prepared_request_has_no_owning_mutation_before_handoff);
    RUN_TEST(test_confirmed_manual_start_reaches_existing_owning_persist_path);
    RUN_TEST(test_confirmed_start_reuses_prepared_envelope_monotonic_time);
    RUN_TEST(test_confirmed_stop_reaches_existing_owning_persist_path);
    RUN_TEST(test_revalidation_failure_does_not_enter_owning_path);
    RUN_TEST(test_duplicate_confirmed_request_preserves_owner_idempotency);
    RUN_TEST(test_acknowledge_message_is_ram_owned_and_persistence_ineligible);
    RUN_TEST(test_mute_message_is_ram_owned_and_persistence_ineligible);
    RUN_TEST(
        test_selected_message_row_reaches_the_owning_acknowledge_and_mute_path);
    RUN_TEST(test_language_row_press_reaches_the_owning_configuration_commit);
    RUN_TEST(test_refused_language_change_is_visible_and_cleared_by_a_success);
    RUN_TEST(
        test_device_name_commit_reaches_the_owner_and_an_active_run_refuses_it);
    RUN_TEST(test_program_save_outcome_decides_whether_the_editor_is_clean);
    RUN_TEST(test_command_status_projection_keeps_decisions_only);
    RUN_TEST(test_product_inserted_without_runtime_context_is_context_missing);
    RUN_TEST(
        test_product_inserted_waiting_for_product_reaches_target_and_persists);
    RUN_TEST(test_product_inserted_in_wrong_state_is_rejected_without_change);
    RUN_TEST(test_product_inserted_stale_sequence_is_rejected_without_change);
    RUN_TEST(
        test_product_inserted_persistence_failure_is_fail_closed_and_retryable);
    RUN_TEST(test_product_inserted_repeated_press_is_rejected);
    RUN_TEST(test_dispatch_transition_action_reaches_the_owning_application);
    RUN_TEST(test_dispatch_transition_action_without_context_is_decision_only);
    RUN_TEST(test_program_copy_and_new_create_user_programs);
    RUN_TEST(
        test_program_delete_removes_user_program_but_never_a_factory_program);
    RUN_TEST(
        test_program_uninstall_needs_confirmation_and_reset_restores_factory);
    RUN_TEST(test_program_edit_of_the_active_program_is_blocked_before_preview);
    RUN_TEST(test_program_edit_and_reset_of_the_active_program_are_blocked);
    RUN_TEST(test_program_edit_with_stale_catalog_revision_is_rejected);
    RUN_TEST(
        test_program_edit_from_a_stale_user_configuration_snapshot_is_rejected);
    RUN_TEST(test_program_edit_without_expected_revisions_is_fail_closed);
    RUN_TEST(test_confirm_product_inserted_waits_for_the_application_gate);
    RUN_TEST(test_apply_program_edit_waits_for_the_application_gate);
    RUN_TEST(test_program_capacity_is_reported_and_recoverable);
    RUN_TEST(test_program_edit_without_run_state_or_service_is_fail_closed);
    RUN_TEST(test_program_edit_persists_across_a_restart);
    RUN_TEST(test_dispatch_program_edit_reaches_the_owning_application);
    RUN_TEST(
        test_dispatch_network_touch_actions_use_existing_application_bridge);
    RUN_TEST(test_web_access_open_runs_through_bridge_to_the_application_owner);
    RUN_TEST(
        test_web_access_provisioning_success_is_projected_as_not_applicable);
    RUN_TEST(test_web_access_not_allowed_state_is_not_bypassed_by_the_ui);
    RUN_TEST(test_process_touch_without_contact_yields_no_target);
    RUN_TEST(test_process_touch_held_without_fresh_edge_does_not_navigate);
    RUN_TEST(test_process_touch_fresh_edge_on_valid_slot_navigates);
    RUN_TEST(
        test_process_touch_from_home_reaches_network_page_and_application_owner);
    RUN_TEST(test_process_touch_fresh_edge_off_target_does_not_navigate);
    RUN_TEST(test_manual_start_has_no_path_without_an_owner_of_the_run_limits);
    RUN_TEST(test_local_service_pin_grants_the_lease_only_after_a_correct_pin);
    RUN_TEST(
        test_local_service_lockout_holds_during_the_wait_and_after_restart);
    RUN_TEST(test_local_service_pin_without_provisioning_runs_no_kdf);
    RUN_TEST(test_local_service_pin_is_refused_outside_validated_standby);
    RUN_TEST(test_local_service_pin_refuses_a_run_started_during_the_check);
    RUN_TEST(test_local_service_lease_ends_after_ten_minutes_without_activity);
    RUN_TEST(test_local_service_sign_out_ends_the_lease);
    RUN_TEST(test_local_service_lease_ends_for_good_when_standby_is_left);
    RUN_TEST(test_local_service_lease_ends_on_service_required_and_recovery);
    RUN_TEST(test_local_service_lease_ends_on_authentication_reinitialisation);
    return UNITY_END();
}
