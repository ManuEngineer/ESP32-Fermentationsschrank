#include <unity.h>

#include <type_traits>
#include <variant>

#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "device_platform.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"
#include "standard_program_catalog.hpp"

namespace {

using namespace fermentation;

FermentationApplicationOwningEvidence uiEvidence(bool safetyAllowsStart = true) {
    FermentationApplicationOwningEvidence evidence;
    evidence.safetyAllowsStart = safetyAllowsStart;
    evidence.airSensorValid = true;
    evidence.coolingSensorValid = true;
    evidence.productSensorValid = true;
    return evidence;
}

RunCommandState standbyState() {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    return state;
}

FermentationUiCommandContext context(const RunCommandState& state,
                                     bool confirmed = false) {
    FermentationUiCommandContext value;
    value.surface = device_platform::UiSurface::LocalDisplay;
    value.monotonicMillis = 100U;
    value.expected.expectedStateSequence =
        state.processState.transitionSequence;
    value.expected.expectedRunRevision = state.runRevision;
    value.expected.expectedMessageRevision = state.messageRevision;
    value.expected.expectedFaultRevision = state.faultRevision;
    value.confirmed = confirmed;
    return value;
}

std::optional<FermentationUiConfirmationRequest> confirmation(
    const FermentationUiCommandContext& value) {
    return FermentationUiCommandBridge::confirmationRequest(
        value, FermentationUiAction::StartProgram,
        {device_platform::TextNamespace{"fermentation"},
         "ui.command.start.title"},
        {device_platform::TextNamespace{"fermentation"},
         "ui.command.start.summary"});
}

CommandStatus commandDetail(const FermentationUiCommandResult& result) {
    TEST_ASSERT_TRUE(std::holds_alternative<CommandStatus>(result.detail));
    return std::get<CommandStatus>(result.detail);
}

void assertDecisionOnly(const FermentationUiCommandResult& result) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::DecisionOnly),
        static_cast<int>(result.phase));
}

void test_ui_request_id_is_the_existing_command_id() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    FermentationUiStartManualHoldingIntent manual;
    manual.plan.targetTemperatureCelsius = 30.0;
    manual.plan.qualificationBandCelsius = 0.5;
    manual.plan.qualificationDurationMinutes = 10U;
    manual.plan.maximumTargetReachMinutes = 60U;
    FermentationUiCommandContext value;
    value.surface = device_platform::UiSurface::WebInterface;
    value.monotonicMillis = 100U;
    const auto prepared = application.prepareStartManualHolding(
        value, manual, uiEvidence());
    TEST_ASSERT_TRUE(prepared.request.has_value());
    TEST_ASSERT_TRUE(prepared.uiRequestId.has_value());
    TEST_ASSERT_EQUAL_UINT64(prepared.uiRequestId->value,
                             prepared.request->commandEnvelope().id);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandSource::WebInterface),
                          static_cast<int>(prepared.request->commandEnvelope().source));
    TEST_ASSERT_FALSE(prepared.request->commandEnvelope().confirmed);
}

void test_canonical_validation_precedes_ui_confirmation() {
    const auto state = standbyState();
    const auto unconfirmedContext = context(state, false);
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    FermentationUiStartManualHoldingIntent manual;
    manual.plan.targetTemperatureCelsius = 30.0;
    manual.plan.qualificationBandCelsius = 0.5;
    manual.plan.qualificationDurationMinutes = 10U;
    manual.plan.maximumTargetReachMinutes = 60U;
    const auto unconfirmedPrepared = application.prepareStartManualHolding(
        unconfirmedContext, manual, uiEvidence());
    TEST_ASSERT_TRUE(unconfirmedPrepared.request.has_value());
    const auto unconfirmed = FermentationUiCommandBridge::decidePrepared(
        state, *unconfirmedPrepared.request,
        confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiCommandOutcomeCategory::
                             ConfirmationRequired),
        static_cast<int>(unconfirmed.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                          static_cast<int>(commandDetail(unconfirmed)));
    assertDecisionOnly(unconfirmed);
    TEST_ASSERT_TRUE(unconfirmed.confirmation.has_value());

    auto staleContext = unconfirmedContext;
    staleContext.expected.expectedRunRevision = 1U;
    const auto stalePrepared = application.prepareStartManualHolding(
        staleContext, manual, uiEvidence());
    TEST_ASSERT_TRUE(stalePrepared.request.has_value());
    const auto stale = FermentationUiCommandBridge::decidePrepared(
        state, *stalePrepared.request, confirmation(staleContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(stale.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::StaleState),
                          static_cast<int>(commandDetail(stale)));
    assertDecisionOnly(stale);
    TEST_ASSERT_FALSE(stale.confirmation.has_value());

    auto invalidManual = manual;
    invalidManual.plan.targetTemperatureCelsius = 0.0;
    const auto invalidPrepared = application.prepareStartManualHolding(
        unconfirmedContext, invalidManual, uiEvidence());
    TEST_ASSERT_TRUE(invalidPrepared.request.has_value());
    const auto invalidResult = FermentationUiCommandBridge::decidePrepared(
        state, *invalidPrepared.request, confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(invalidResult.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                          static_cast<int>(commandDetail(invalidResult)));
    assertDecisionOnly(invalidResult);
    TEST_ASSERT_FALSE(invalidResult.confirmation.has_value());

    const auto unsafePrepared = application.prepareStartManualHolding(
        unconfirmedContext, manual, uiEvidence(false));
    TEST_ASSERT_TRUE(unsafePrepared.request.has_value());
    const auto unsafeResult = FermentationUiCommandBridge::decidePrepared(
        state, *unsafePrepared.request, confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(unsafeResult.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                          static_cast<int>(commandDetail(unsafeResult)));
    assertDecisionOnly(unsafeResult);
    TEST_ASSERT_FALSE(unsafeResult.confirmation.has_value());
}

void test_command_result_preserves_typed_app_details() {
    const auto accepted = FermentationUiCommandBridge::fromCommandStatus(
        CommandStatus::AlreadyProcessed);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Accepted),
        static_cast<int>(accepted.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::AlreadyProcessed),
                          static_cast<int>(commandDetail(accepted)));

    const auto persisted =
        FermentationUiCommandBridge::fromRunPersistenceResult(
            RunPersistenceResultStatus::AlreadyPersisted);
    TEST_ASSERT_TRUE(
        std::holds_alternative<RunPersistenceResultStatus>(persisted.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::AlreadyPersisted),
        static_cast<int>(
            std::get<RunPersistenceResultStatus>(persisted.detail)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
        static_cast<int>(persisted.phase));

    const auto unsupported =
        FermentationUiCommandBridge::unsupportedAppDetail();
    assertDecisionOnly(unsupported);

    const auto preview = FermentationUiCommandBridge::fromConfigurationPreview(
        ConfigurationPreviewStatus::PreviewSuperseded);
    TEST_ASSERT_TRUE(
        std::holds_alternative<ConfigurationPreviewStatus>(preview.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::PreviewSuperseded),
        static_cast<int>(std::get<ConfigurationPreviewStatus>(preview.detail)));

    const auto commit = FermentationUiCommandBridge::fromConfigurationCommit(
        ConfigurationCommitStatus::ConfigurationMutationBusy);
    TEST_ASSERT_TRUE(
        std::holds_alternative<ConfigurationCommitStatus>(commit.detail));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::ConfigurationMutationBusy),
        static_cast<int>(std::get<ConfigurationCommitStatus>(commit.detail)));
    assertDecisionOnly(commit);
}

void test_ui_payloads_are_intents_and_not_owning_evidence() {
    static_assert(!std::is_constructible_v<FermentationUiEnvelopePayload,
                                           ProgramStartRequest>);
    static_assert(!std::is_constructible_v<FermentationUiEnvelopePayload,
                                           ManualStartRequest>);
    static_assert(!std::is_constructible_v<FermentationUiEnvelopePayload,
                                           SensorSelectionCommandRequest>);

    FermentationUiStartProgramIntent start;
    start.programId = "water-kefir";
    start.sensorMode = RunSensorMode::Product;
    FermentationUiEnvelopePayload payload = start;
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiStartProgramIntent>(payload));
    TEST_ASSERT_EQUAL_STRING(
        "water-kefir",
        std::get<FermentationUiStartProgramIntent>(payload).programId.c_str());

    FermentationUiSensorSelectionIntent selection;
    selection.action = SensorSelectionUserAction::RecheckProduct;
    payload = selection;
    TEST_ASSERT_TRUE(
        std::holds_alternative<FermentationUiSensorSelectionIntent>(payload));
}

void test_proposed_decision_is_not_reported_as_applied() {
    const auto proposed =
        FermentationUiCommandBridge::fromCommandStatus(CommandStatus::Proposed);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Accepted),
        static_cast<int>(proposed.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Proposed),
                          static_cast<int>(commandDetail(proposed)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FermentationUiCommandPhase::DecisionOnly),
        static_cast<int>(proposed.phase));
}

void test_prepared_message_actions_remain_bound_to_their_action() {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));

    auto state = standbyState();
    state.messageCount = 1U;
    state.messages[0].id = 7U;
    state.messages[0].acknowledged = true;
    state.messages[0].acousticMuted = false;

    FermentationUiCommandContext value = context(state, false);
    value.expected.expectedMessageRevision = state.messageRevision;
    FermentationApplicationOwningEvidence evidence;
    const auto acknowledge = application.prepareEnvelope(
        value, FermentationUiEnvelopePayload{
                  FermentationUiAcknowledgeMessageIntent{7U}},
        evidence);
    const auto mute = application.prepareEnvelope(
        value,
        FermentationUiEnvelopePayload{FermentationUiMuteMessageIntent{7U}},
        evidence);
    TEST_ASSERT_TRUE(acknowledge.request.has_value());
    TEST_ASSERT_TRUE(mute.request.has_value());

    const auto confirmedAcknowledge =
        FermentationApplication::confirmPrepared(acknowledge);
    const auto confirmedMute = FermentationApplication::confirmPrepared(mute);
    const auto acknowledgeResult = FermentationUiCommandBridge::decidePrepared(
        state, *confirmedAcknowledge.request);
    const auto muteResult = FermentationUiCommandBridge::decidePrepared(
        state, *confirmedMute.request);

    // The acknowledged message makes Ack a NoChange, while the independent
    // acoustic flag makes Mute a Proposed decision. If the prepared payload
    // were reinterpreted by the bridge, these outcomes would be swapped.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NoChange),
                          static_cast<int>(commandDetail(acknowledgeResult)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Proposed),
                          static_cast<int>(commandDetail(muteResult)));
    assertDecisionOnly(acknowledgeResult);
    assertDecisionOnly(muteResult);
    TEST_ASSERT_NOT_EQUAL(acknowledge.request->commandId(),
                          mute.request->commandId());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ui_request_id_is_the_existing_command_id);
    RUN_TEST(test_canonical_validation_precedes_ui_confirmation);
    RUN_TEST(test_command_result_preserves_typed_app_details);
    RUN_TEST(test_ui_payloads_are_intents_and_not_owning_evidence);
    RUN_TEST(test_proposed_decision_is_not_reported_as_applied);
    RUN_TEST(test_prepared_message_actions_remain_bound_to_their_action);
    return UNITY_END();
}
