#include <unity.h>

#include <type_traits>
#include <variant>

#include "fermentation_ui_commands.hpp"
#include "standard_program_catalog.hpp"

namespace {

using namespace fermentation;

RunCommandState standbyState() {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    return state;
}

ProgramStartRequest validProgramStart() {
    auto program = FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(program.has_value());
    program->program.productSensorFailure.fallbackDelaySeconds = 30U;
    program->program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program->program.fermentationStages.front().durationMinutes = 120U;
    program->program.targetQualification.bandCelsius = 0.5;
    program->program.targetQualification.durationMinutes = 10U;
    program->program.maximumTargetReachMinutes = 180U;
    if (program->program.preheat) {
        program->program.maximumProductWaitMinutes = 30U;
    }
    if (program->program.completion.mode !=
        CompletionMode::FinishWithoutCooling) {
        program->program.completion.coolingTargetCelsius = 8.0;
    }
    TEST_ASSERT_TRUE(validateProgram(*program).valid());

    ProgramStartRequest request;
    request.runId = "ui-run";
    request.program = *program;
    request.sourceKind = ProgramSourceKind::FactoryCatalog;
    request.sourceProgramRevision =
        ::fermentation::RunProgramSourceRevision{1U};
    request.sensorMode = RunSensorMode::Product;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = true;
    return request;
}

FermentationUiCommandContext context(const RunCommandState& state,
                                     bool confirmed = false) {
    FermentationUiCommandContext value;
    value.surface = device_platform::UiSurface::LocalDisplay;
    value.requestId.value = 42U;
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

void test_ui_request_id_is_the_existing_command_id() {
    const auto state = standbyState();
    const auto value = context(state);
    const auto envelope = FermentationUiCommandBridge::makeEnvelope(value);

    TEST_ASSERT_EQUAL_UINT64(value.requestId.value, envelope.id);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandSource::LocalDisplay),
                          static_cast<int>(envelope.source));
    TEST_ASSERT_FALSE(envelope.confirmed);
}

void test_canonical_validation_precedes_ui_confirmation() {
    const auto state = standbyState();
    const auto unconfirmedContext = context(state, false);

    const auto unconfirmed = FermentationUiCommandBridge::decideProgramStart(
        state, validProgramStart(), unconfirmedContext,
        confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::DeviceUiCommandOutcomeCategory::
                             ConfirmationRequired),
        static_cast<int>(unconfirmed.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                          static_cast<int>(commandDetail(unconfirmed)));
    TEST_ASSERT_TRUE(unconfirmed.confirmation.has_value());

    auto staleContext = unconfirmedContext;
    staleContext.expected.expectedRunRevision = 1U;
    const auto stale = FermentationUiCommandBridge::decideProgramStart(
        state, validProgramStart(), staleContext, confirmation(staleContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(stale.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::StaleState),
                          static_cast<int>(commandDetail(stale)));
    TEST_ASSERT_FALSE(stale.confirmation.has_value());

    auto invalid = validProgramStart();
    invalid.sourceProgramRevision =
        ::fermentation::RunProgramSourceRevision{0U};
    const auto invalidResult = FermentationUiCommandBridge::decideProgramStart(
        state, invalid, unconfirmedContext, confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(invalidResult.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                          static_cast<int>(commandDetail(invalidResult)));
    TEST_ASSERT_FALSE(invalidResult.confirmation.has_value());

    auto unsafe = validProgramStart();
    unsafe.safetyAllowsStart = false;
    const auto unsafeResult = FermentationUiCommandBridge::decideProgramStart(
        state, unsafe, unconfirmedContext, confirmation(unconfirmedContext));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::DeviceUiCommandOutcomeCategory::Rejected),
        static_cast<int>(unsafeResult.category));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                          static_cast<int>(commandDetail(unsafeResult)));
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
    return UNITY_END();
}
