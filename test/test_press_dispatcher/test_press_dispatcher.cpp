#include <unity.h>

#include <optional>
#include <variant>

#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_text.hpp"
#include "mock_time_zone_resolver.hpp"
#include "mock_network_lifecycle.hpp"
#include "run_persistence_codec.hpp"
#include "run_persistence_coordinator.hpp"
#include "simulated_persistent_state_store.hpp"
#include "state_store_key.hpp"
#include "virtual_time_source.hpp"

#include "../../main/fermentation_ui_press_dispatcher.hpp"

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    static RunCommandState& runtimeState(FermentationApplication& application) {
        return *application.runtimeRunState_;
    }

    static RunPersistenceCoordinator& runPersistenceCoordinator(
        FermentationApplication& application) {
        return *application.runPersistenceCoordinator_;
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
        running_ = false;
        return true;
    }
    [[nodiscard]] bool running() const override { return running_; }

   private:
    bool running_{false};
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

void test_dispatch_action_reaches_prepare_and_confirm() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    press.action = FermentationUiEnvelopePayload{
        FermentationUiStartManualTimedIntent{validManualTimedValues()}};
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
    // confirmPrepared() is genuinely, correctly reached (proving both
    // prepare AND confirm are wired, not just prepare): revalidatePrepared
    // Request() re-checks safety/sensor evidence, and this minimal fixture
    // (a bare begin(), no update() tick, no published sensor evidence) has
    // none yet, so the existing, unmodified application logic itself
    // rejects with Unavailable - this dispatcher invents nothing and
    // forwards that real outcome untouched.
    TEST_ASSERT_TRUE(result.confirmStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationApplicationRequestStatus::Unavailable),
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
    const FermentationUiEnvelopePayload payload =
        FermentationUiStartManualTimedIntent{validManualTimedValues()};

    const auto prepared = fixture.application.prepareEnvelope(context, payload);
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
    const auto snapshot = fixture.application.uiSnapshot();
    FermentationUiWorkspacePress press;
    press.action = FermentationUiEnvelopePayload{
        FermentationUiStartManualTimedIntent{validManualTimedValues()}};

    const auto result =
        dispatchWorkspacePress(fixture.application, snapshot, press,
                               fixture.timeSource.monotonicMillis());
    assertAppliedOwningResult(result);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationHomeMode::ActiveRun),
        static_cast<int>(fixture.application.uiSnapshot().home.mode));
}

void test_confirmed_start_reuses_prepared_envelope_monotonic_time() {
    OwningAppFixture fixture;
    const auto snapshot = fixture.application.uiSnapshot();
    FermentationUiCommandContext context;
    context.expected = snapshot.revisions;
    context.monotonicMillis = fixture.timeSource.monotonicMillis();
    const FermentationUiEnvelopePayload payload =
        FermentationUiStartManualTimedIntent{validManualTimedValues()};

    const auto prepared = fixture.application.prepareEnvelope(context, payload);
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
    FermentationUiWorkspacePress start;
    start.action = FermentationUiEnvelopePayload{
        FermentationUiStartManualTimedIntent{validManualTimedValues()}};
    const auto started = dispatchWorkspacePress(
        fixture.application, fixture.application.uiSnapshot(), start,
        fixture.timeSource.monotonicMillis());
    assertAppliedOwningResult(started);

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
    const auto snapshot = fixture.application.uiSnapshot();
    FermentationUiCommandContext context;
    context.expected = snapshot.revisions;
    context.monotonicMillis = fixture.timeSource.monotonicMillis();
    const FermentationUiEnvelopePayload payload =
        FermentationUiStartManualTimedIntent{validManualTimedValues()};
    const auto prepared = fixture.application.prepareEnvelope(context, payload);
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
    const auto snapshot = fixture.application.uiSnapshot();
    FermentationUiCommandContext context;
    context.expected = snapshot.revisions;
    context.monotonicMillis = fixture.timeSource.monotonicMillis();
    const FermentationUiEnvelopePayload payload =
        FermentationUiStartManualTimedIntent{validManualTimedValues()};
    const auto prepared = fixture.application.prepareEnvelope(context, payload);
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

void test_dispatch_transition_action_is_unavailable_no_owner() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    press.transitionAction = FermentationUiProductInsertedConfirmedIntent{};

    const auto result =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::UnavailableNoOwner),
        static_cast<int>(result.outcome));
}

void test_dispatch_program_edit_is_unavailable_no_owner() {
    AppFixture fixture;
    FermentationUiWorkspacePress press;
    press.programEdit = FermentationUiProgramEditRequest{
        FermentationUiProgramEditOperation::Reset, "p1", std::nullopt,
        std::nullopt, true};

    const auto result =
        dispatchWorkspacePress(fixture.application, {}, press, 1000U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WorkspacePressDispatchOutcome::UnavailableNoOwner),
        static_cast<int>(result.outcome));
}

void test_dispatch_network_touch_actions_use_existing_application_bridge() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    MockHttpServerLifecycle http;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                       timeSource, network, http));

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

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_dispatch_no_typed_payload_is_reported_as_such);
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
    RUN_TEST(test_command_status_projection_keeps_decisions_only);
    RUN_TEST(test_dispatch_transition_action_is_unavailable_no_owner);
    RUN_TEST(test_dispatch_program_edit_is_unavailable_no_owner);
    RUN_TEST(
        test_dispatch_network_touch_actions_use_existing_application_bridge);
    RUN_TEST(test_process_touch_without_contact_yields_no_target);
    RUN_TEST(test_process_touch_held_without_fresh_edge_does_not_navigate);
    RUN_TEST(test_process_touch_fresh_edge_on_valid_slot_navigates);
    RUN_TEST(test_process_touch_fresh_edge_off_target_does_not_navigate);
    return UNITY_END();
}
