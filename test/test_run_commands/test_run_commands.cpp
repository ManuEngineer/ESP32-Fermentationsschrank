#include <unity.h>

#include <array>
#include <cstdint>
#include <limits>

#include "run_commands.hpp"
#include "sensor_selection.hpp"
#include "standard_program_catalog.hpp"

namespace {

using namespace fermentation;
using device_platform::SensorFaultReason;
using device_platform::SensorQuality;
using device_platform::SensorQualitySnapshot;

ProgramDocument commissionProgram(const char* id) {
    auto document = FactoryProgramCatalog::find(id);
    TEST_ASSERT_TRUE(document.has_value());
    auto& program = document->program;
    program.productSensorFailure.fallbackDelaySeconds = 30U;
    program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program.fermentationStages.front().durationMinutes = 120U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    if (program.preheat) {
        program.maximumProductWaitMinutes = 30U;
    }
    if (program.completion.mode != CompletionMode::FinishWithoutCooling) {
        program.completion.coolingTargetCelsius = 8.0;
    }
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    return *document;
}

RunCommandState standbyState() {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    return state;
}

CommandEnvelope envelope(CommandId id, const RunCommandState& state,
                         CommandSource source = CommandSource::LocalDisplay,
                         bool confirmed = true,
                         std::uint64_t monotonicMillis = 100U) {
    CommandEnvelope value;
    value.id = id;
    value.source = source;
    value.monotonicMillis = monotonicMillis;
    value.expectedStateSequence = state.processState.transitionSequence;
    value.expectedRunRevision = state.runRevision;
    value.expectedMessageRevision = state.messageRevision;
    value.expectedFaultRevision = state.faultRevision;
    value.confirmed = confirmed;
    return value;
}

ManualRunPlanRequest manualPlan(const char* id, bool preheat = false) {
    ManualRunPlanRequest plan;
    plan.runId = id;
    plan.targetTemperatureCelsius = 12.0;
    plan.sensorMode = RunSensorMode::Air;
    plan.preheatEnabled = preheat;
    if (preheat) {
        plan.maximumProductWaitMinutes = 30U;
    }
    plan.qualificationBandCelsius = 0.5;
    plan.qualificationDurationMinutes = 10U;
    plan.maximumTargetReachMinutes = 180U;
    return plan;
}

ProgramStartRequest programStart(
    const RunCommandState& state, CommandId id,
    CommandSource source = CommandSource::LocalDisplay) {
    ProgramStartRequest request;
    request.envelope = envelope(id, state, source);
    request.runId = "run-15";
    request.program = commissionProgram("water-kefir");
    request.sourceKind = ProgramSourceKind::FactoryCatalog;
    request.sourceProgramRevision = 1U;
    request.sensorMode = RunSensorMode::Product;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = true;
    return request;
}

ManualStartRequest manualStart(const RunCommandState& state, CommandId id,
                               ManualRunPlanRequest plan,
                               bool safetyAllowsStart = true,
                               bool airSensorValid = true,
                               bool coolingSensorValid = true,
                               bool productSensorValid = true) {
    ManualStartRequest request;
    request.envelope = envelope(id, state);
    request.plan = std::move(plan);
    request.safetyAllowsStart = safetyAllowsStart;
    request.airSensorValid = airSensorValid;
    request.coolingSensorValid = coolingSensorValid;
    request.productSensorValid = productSensorValid;
    return request;
}

RunCommandState startedProgramState(const char* programId = "water-kefir") {
    auto state = standbyState();
    auto request = programStart(state, 1U);
    request.program = commissionProgram(programId);
    const auto decision = decideProgramStart(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(state, decision)));
    return state;
}

bool hasEffect(const CommandDecision& decision, CommandEffect effect) {
    for (std::size_t index = 0U; index < decision.effectCount; ++index) {
        if (decision.effects[index] == effect) {
            return true;
        }
    }
    return false;
}

bool equalProgramDocuments(const ProgramDocument& left,
                           const ProgramDocument& right) {
    const auto& leftProgram = left.program;
    const auto& rightProgram = right.program;
    if (left.schema.version != right.schema.version ||
        left.schema.presentFields != right.schema.presentFields ||
        leftProgram.id != rightProgram.id ||
        leftProgram.name != rightProgram.name ||
        leftProgram.notes != rightProgram.notes ||
        leftProgram.builtIn != rightProgram.builtIn ||
        leftProgram.factoryCatalogEntry != rightProgram.factoryCatalogEntry ||
        leftProgram.resettable != rightProgram.resettable ||
        leftProgram.userDeletable != rightProgram.userDeletable ||
        leftProgram.installed != rightProgram.installed ||
        leftProgram.enabled != rightProgram.enabled ||
        leftProgram.preheat != rightProgram.preheat ||
        leftProgram.sensorPreference != rightProgram.sensorPreference ||
        leftProgram.productSensorFailure.policy !=
            rightProgram.productSensorFailure.policy ||
        leftProgram.productSensorFailure.fallbackDelaySeconds !=
            rightProgram.productSensorFailure.fallbackDelaySeconds ||
        leftProgram.targetQualification.bandCelsius !=
            rightProgram.targetQualification.bandCelsius ||
        leftProgram.targetQualification.durationMinutes !=
            rightProgram.targetQualification.durationMinutes ||
        leftProgram.maximumTargetReachMinutes !=
            rightProgram.maximumTargetReachMinutes ||
        leftProgram.maximumProductWaitMinutes !=
            rightProgram.maximumProductWaitMinutes ||
        leftProgram.completion.mode != rightProgram.completion.mode ||
        leftProgram.completion.coolingTargetCelsius !=
            rightProgram.completion.coolingTargetCelsius ||
        leftProgram.completion.holdDurationMinutes !=
            rightProgram.completion.holdDurationMinutes ||
        leftProgram.fermentationStages.size() !=
            rightProgram.fermentationStages.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < leftProgram.fermentationStages.size();
         ++index) {
        if (leftProgram.fermentationStages[index].targetTemperatureCelsius !=
                rightProgram.fermentationStages[index]
                    .targetTemperatureCelsius ||
            leftProgram.fermentationStages[index].durationMinutes !=
                rightProgram.fermentationStages[index].durationMinutes) {
            return false;
        }
    }
    return true;
}

bool equalEffectiveRunValues(const EffectiveRunValues& left,
                             const EffectiveRunValues& right) {
    return left.targetTemperatureCelsius == right.targetTemperatureCelsius &&
           left.remainingDurationMinutes == right.remainingDurationMinutes;
}

bool equalRunRevisions(const RunRevision& left, const RunRevision& right) {
    return left.sequence == right.sequence &&
           left.monotonicEpoch == right.monotonicEpoch &&
           left.stageIndex == right.stageIndex &&
           left.completedStageCount == right.completedStageCount &&
           equalEffectiveRunValues(left.before, right.before) &&
           equalEffectiveRunValues(left.after, right.after) &&
           left.targetTemperatureChanged == right.targetTemperatureChanged &&
           left.remainingDurationChanged == right.remainingDurationChanged &&
           left.effect == right.effect && left.source == right.source &&
           left.reason == right.reason &&
           left.timestamp.monotonicMillis == right.timestamp.monotonicMillis &&
           left.timestamp.unixTimeSeconds == right.timestamp.unixTimeSeconds;
}

bool equalActiveRuns(const std::optional<ActiveRun>& left,
                     const std::optional<ActiveRun>& right) {
    if (left.has_value() != right.has_value()) {
        return false;
    }
    if (!left.has_value()) {
        return true;
    }
    const auto& leftSnapshot = left->snapshot();
    const auto& rightSnapshot = right->snapshot();
    if (!equalProgramDocuments(leftSnapshot.sourceProgram,
                               rightSnapshot.sourceProgram) ||
        leftSnapshot.sourceKind != rightSnapshot.sourceKind ||
        leftSnapshot.sourceProgramRevision !=
            rightSnapshot.sourceProgramRevision ||
        !equalEffectiveRunValues(left->effectiveValues(),
                                 right->effectiveValues()) ||
        left->revisionCount() != right->revisionCount()) {
        return false;
    }
    for (std::size_t index = 0U; index < kMaximumRunRevisions; ++index) {
        if (!equalRunRevisions(left->revisions()[index],
                               right->revisions()[index])) {
            return false;
        }
    }
    return true;
}

bool equalManualRunPlanRequests(const ManualRunPlanRequest& left,
                                const ManualRunPlanRequest& right) {
    return left.runId == right.runId &&
           left.targetTemperatureCelsius == right.targetTemperatureCelsius &&
           left.sensorMode == right.sensorMode &&
           left.preheatEnabled == right.preheatEnabled &&
           left.maximumProductWaitMinutes == right.maximumProductWaitMinutes &&
           left.qualificationBandCelsius == right.qualificationBandCelsius &&
           left.qualificationDurationMinutes ==
               right.qualificationDurationMinutes &&
           left.maximumTargetReachMinutes == right.maximumTargetReachMinutes;
}

bool equalManualRuns(const std::optional<ManualRunPlan>& left,
                     const std::optional<ManualRunPlan>& right) {
    if (left.has_value() != right.has_value()) {
        return false;
    }
    return !left.has_value() ||
           (equalManualRunPlanRequests(left->values, right->values) &&
            left->source == right->source &&
            left->createdAtMonotonicMillis == right->createdAtMonotonicMillis &&
            left->kind == right->kind);
}

bool equalProcessRunSnapshots(const std::optional<ProcessRunSnapshot>& left,
                              const std::optional<ProcessRunSnapshot>& right) {
    if (left.has_value() != right.has_value()) {
        return false;
    }
    return !left.has_value() ||
           (left->kind == right->kind &&
            left->preheatEnabled == right->preheatEnabled &&
            left->completionMode == right->completionMode &&
            left->qualificationDurationMinutes ==
                right->qualificationDurationMinutes &&
            left->maximumTargetReachMinutes ==
                right->maximumTargetReachMinutes &&
            left->maximumProductWaitMinutes ==
                right->maximumProductWaitMinutes &&
            left->fermentationDurationMinutes ==
                right->fermentationDurationMinutes &&
            left->holdDurationMinutes == right->holdDurationMinutes);
}

bool equalRuntimeMessages(const RuntimeMessage& left,
                          const RuntimeMessage& right) {
    return left.id == right.id && left.code == right.code &&
           left.messageClass == right.messageClass &&
           left.priority == right.priority && left.trigger == right.trigger &&
           left.monotonicMillis == right.monotonicMillis &&
           left.active == right.active &&
           left.acknowledged == right.acknowledged &&
           left.resolved == right.resolved &&
           left.decisionRequired == right.decisionRequired &&
           left.acousticMuted == right.acousticMuted &&
           left.acousticIntent == right.acousticIntent &&
           left.runRevision == right.runRevision &&
           left.stateSequence == right.stateSequence &&
           left.faultRevision == right.faultRevision &&
           left.revision == right.revision;
}

bool equalRunCommandStates(const RunCommandState& left,
                           const RunCommandState& right) {
    if (!equalProcessRuntimeState(left.processState, right.processState) ||
        !equalActiveRuns(left.activeProgramRun, right.activeProgramRun) ||
        !equalManualRuns(left.activeManualRun, right.activeManualRun) ||
        !equalProcessRunSnapshots(left.processRunSnapshot,
                                  right.processRunSnapshot) ||
        left.activeRunId != right.activeRunId ||
        left.activeRunSensorMode != right.activeRunSensorMode ||
        left.sensorSelection != right.sensorSelection ||
        left.sensorSelectionRuntime != right.sensorSelectionRuntime ||
        left.runRevision != right.runRevision ||
        left.messageCount != right.messageCount ||
        left.messageRevision != right.messageRevision ||
        left.faultRevision != right.faultRevision ||
        left.criticalSafetyEventPending != right.criticalSafetyEventPending ||
        left.commandSequence != right.commandSequence ||
        left.lastCommandMonotonicMillis != right.lastCommandMonotonicMillis ||
        left.processedCommandCount != right.processedCommandCount) {
        return false;
    }
    for (std::size_t index = 0U; index < left.messages.size(); ++index) {
        if (!equalRuntimeMessages(left.messages[index],
                                  right.messages[index])) {
            return false;
        }
    }
    for (std::size_t index = 0U; index < left.processedCommandIds.size();
         ++index) {
        if (left.processedCommandIds[index] !=
            right.processedCommandIds[index]) {
            return false;
        }
    }
    return true;
}

void assertRejectedWithoutStateMutation(const CommandDecision& decision) {
    TEST_ASSERT_FALSE(decision.proposed());
    TEST_ASSERT_TRUE(equalRunCommandStates(decision.before, decision.after));
    TEST_ASSERT_EQUAL_UINT32(0U, decision.effectCount);
}

RuntimeMessage message(std::uint32_t id, MessageCode code,
                       MessageClass messageClass, std::uint8_t priority,
                       MessageTrigger trigger) {
    RuntimeMessage value;
    value.id = id;
    value.code = code;
    value.messageClass = messageClass;
    value.priority = priority;
    value.trigger = trigger;
    return value;
}

void test_program_start_requires_confirmation_safety_and_current_context() {
    const auto state = standbyState();
    auto request = programStart(state, 1U);

    request.envelope.confirmed = false;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::NotConfirmed),
        static_cast<int>(decideProgramStart(state, request).status));
    request.envelope.confirmed = true;
    request.safetyAllowsStart = false;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::SafetyRejected),
        static_cast<int>(decideProgramStart(state, request).status));
    request.safetyAllowsStart = true;
    request.envelope.expectedStateSequence = 1U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::StaleState),
        static_cast<int>(decideProgramStart(state, request).status));
    request.envelope.expectedStateSequence = 0U;
    request.sourceProgramRevision = 0U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::InvalidInput),
        static_cast<int>(decideProgramStart(state, request).status));
}

void test_run_id_boundary_is_shared_by_program_and_manual_start() {
    auto state = standbyState();
    auto program = programStart(state, 71U);
    program.runId.clear();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::InvalidInput),
        static_cast<int>(decideProgramStart(state, program).status));

    program.runId.assign(49U, 'x');
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::InvalidInput),
        static_cast<int>(decideProgramStart(state, program).status));

    program.runId.assign(48U, 'x');
    TEST_ASSERT_TRUE(decideProgramStart(state, program).proposed());

    auto manual = manualStart(state, 72U, manualPlan(""));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::InvalidInput),
        static_cast<int>(decideManualStart(state, manual).status));
    manual.plan.runId.assign(49U, 'x');
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::InvalidInput),
        static_cast<int>(decideManualStart(state, manual).status));
}

void test_start_summary_is_available_before_confirmation_but_never_masks_rejections() {
    // Gueltig, unbestaetigt: Zusammenfassung vorhanden, keine Mutation.
    {
        auto state = standbyState();
        auto request = programStart(state, 1U);
        request.envelope.confirmed = false;
        const auto decision = decideProgramStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.startSummary.has_value());
        TEST_ASSERT_EQUAL_STRING("run-15",
                                 decision.startSummary->runId.c_str());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ProcessState::Standby),
            static_cast<int>(decision.after.processState.state));
        TEST_ASSERT_FALSE(decision.after.activeProgramRun.has_value());
        TEST_ASSERT_EQUAL_UINT32(decision.before.runRevision,
                                 decision.after.runRevision);
        assertRejectedWithoutStateMutation(decision);
    }
    // Dieselbe gueltige Anfrage, bestaetigt: gleiche Zusammenfassung, echte
    // Mutation.
    {
        auto state = standbyState();
        const auto decision =
            decideProgramStart(state, programStart(state, 1U));
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_TRUE(decision.startSummary.has_value());
        TEST_ASSERT_EQUAL_STRING("run-15",
                                 decision.startSummary->runId.c_str());
        TEST_ASSERT_TRUE(hasEffect(decision, CommandEffect::RunStarted));
    }
    // Ungueltiges Programm, unbestaetigt: InvalidInput darf nicht durch
    // NotConfirmed maskiert werden.
    {
        auto state = standbyState();
        auto request = programStart(state, 1U);
        request.sourceProgramRevision = 0U;
        request.envelope.confirmed = false;
        const auto decision = decideProgramStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.startSummary.has_value());
    }
    // Veraltete Revision, unbestaetigt: StaleState darf nicht durch
    // NotConfirmed maskiert werden.
    {
        auto state = standbyState();
        auto request = programStart(state, 1U);
        request.envelope.expectedStateSequence = 1U;
        request.envelope.confirmed = false;
        const auto decision = decideProgramStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::StaleState),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.startSummary.has_value());
    }
    // Fehlende Sicherheitsfreigabe, unbestaetigt: SafetyRejected darf nicht
    // durch NotConfirmed maskiert werden.
    {
        auto state = standbyState();
        auto request = programStart(state, 1U);
        request.safetyAllowsStart = false;
        request.envelope.confirmed = false;
        const auto decision = decideProgramStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.startSummary.has_value());
    }
}

void test_manual_start_summary_is_available_before_confirmation_but_never_masks_rejections() {
    // Gueltig, unbestaetigt: Zusammenfassung vorhanden, keine Mutation.
    {
        auto state = standbyState();
        auto request = manualStart(state, 1U, manualPlan("hold"));
        request.envelope.confirmed = false;
        const auto decision = decideManualStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.startSummary.has_value());
        TEST_ASSERT_FALSE(decision.after.activeManualRun.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ProcessState::Standby),
            static_cast<int>(decision.after.processState.state));
        assertRejectedWithoutStateMutation(decision);
    }
    // Ungueltiger Plan (Vorwaerme-Wartezeit fehlt), unbestaetigt: InvalidInput
    // darf nicht durch NotConfirmed maskiert werden.
    {
        auto state = standbyState();
        auto plan = manualPlan("preheat-hold", true);
        plan.maximumProductWaitMinutes.reset();
        auto request = manualStart(state, 1U, plan);
        request.envelope.confirmed = false;
        const auto decision = decideManualStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.startSummary.has_value());
    }
    // Fehlende Sicherheitsfreigabe, unbestaetigt: SafetyRejected darf nicht
    // durch NotConfirmed maskiert werden.
    {
        auto state = standbyState();
        auto request = manualStart(state, 1U, manualPlan("hold"), false);
        request.envelope.confirmed = false;
        const auto decision = decideManualStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.startSummary.has_value());
    }
}

void test_program_start_is_two_stage_and_contains_summary() {
    auto state = standbyState();
    const auto decision = decideProgramStart(state, programStart(state, 1U));

    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.startSummary.has_value());
    TEST_ASSERT_EQUAL_STRING("run-15", decision.startSummary->runId.c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_FALSE(state.activeProgramRun.has_value());
    TEST_ASSERT_TRUE(hasEffect(decision, CommandEffect::RunStarted));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(state, decision)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, state.runRevision);
}

void test_manual_plans_have_no_duration_and_use_canonical_start_states() {
    auto state = standbyState();
    auto withoutPreheat = manualStart(state, 1U, manualPlan("hold"));
    const auto direct = decideManualStart(state, withoutPreheat);
    TEST_ASSERT_TRUE(direct.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(direct.after.processState.state));
    TEST_ASSERT_FALSE(direct.startSummary->durationMinutes.has_value());

    auto withPreheat = manualStart(state, 2U, manualPlan("preheat-hold", true));
    const auto preheated = decideManualStart(state, withPreheat);
    TEST_ASSERT_TRUE(preheated.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(preheated.after.processState.state));

    withPreheat.plan.maximumProductWaitMinutes.reset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::InvalidInput),
        static_cast<int>(decideManualStart(state, withPreheat).status));
}

void test_display_and_web_conflict_is_first_applied_without_source_priority() {
    auto state = standbyState();
    const auto display = decideProgramStart(state, programStart(state, 10U));
    const auto web = decideProgramStart(
        state, programStart(state, 11U, CommandSource::WebInterface));
    TEST_ASSERT_TRUE(display.proposed());
    TEST_ASSERT_TRUE(web.proposed());

    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(state, web)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::StaleState),
                          static_cast<int>(applyRunCommand(state, display)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::AlreadyProcessed),
                          static_cast<int>(applyRunCommand(state, web)));
}

void test_stop_back_is_inert_and_abort_off_is_atomic() {
    auto state = startedProgramState();
    StopRequest back{envelope(2U, state), StopOption::Back, std::nullopt,
                     false};
    const auto backDecision = decideStop(state, back);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NoChange),
                          static_cast<int>(backDecision.status));
    assertRejectedWithoutStateMutation(backDecision);

    StopRequest abort{envelope(3U, state), StopOption::AbortAndTurnOff,
                      std::nullopt, false};
    const auto decision = decideStop(state, abort);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
    TEST_ASSERT_FALSE(decision.after.activeProgramRun.has_value());
    TEST_ASSERT_TRUE(
        hasEffect(decision, CommandEffect::SafePeltierStopRequested));
    TEST_ASSERT_TRUE(hasEffect(decision, CommandEffect::FanRunOnRequired));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(state, decision)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(state.processState.state));
}

void test_stop_rejects_unknown_option_without_mutation() {
    auto state = startedProgramState();
    StopRequest unknown{envelope(2U, state), static_cast<StopOption>(0xFF),
                        std::nullopt, false};
    const auto decision = decideStop(state, unknown);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                          static_cast<int>(decision.status));
    assertRejectedWithoutStateMutation(decision);

    StopRequest valid{envelope(2U, state), StopOption::AbortAndTurnOff,
                      std::nullopt, false};
    const auto validDecision = decideStop(state, valid);
    TEST_ASSERT_TRUE(validDecision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::Applied),
        static_cast<int>(applyRunCommand(state, validDecision)));

    unknown.envelope = envelope(2U, state);
    const auto duplicate = decideStop(state, unknown);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::AlreadyProcessed),
                          static_cast<int>(duplicate.status));
    assertRejectedWithoutStateMutation(duplicate);

    StopRequest duplicateBack{envelope(2U, state), StopOption::Back,
                              std::nullopt, false};
    const auto alreadyProcessedBack = decideStop(state, duplicateBack);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::AlreadyProcessed),
                          static_cast<int>(alreadyProcessedBack.status));
    assertRejectedWithoutStateMutation(alreadyProcessedBack);

    StopRequest freshBack{envelope(3U, state), StopOption::Back, std::nullopt,
                          false};
    const auto noChange = decideStop(state, freshBack);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NoChange),
                          static_cast<int>(noChange.status));
    assertRejectedWithoutStateMutation(noChange);
}

void test_abort_and_cool_validates_replacement_before_commit() {
    auto state = startedProgramState();
    StopRequest request{envelope(2U, state), StopOption::AbortAndCool,
                        manualPlan("cool"), true};
    request.coolingPlan->targetTemperatureCelsius = 100.0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                          static_cast<int>(decideStop(state, request).status));
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());

    request.coolingPlan = manualPlan("cool");
    const auto decision = decideStop(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.before.activeProgramRun.has_value());
    TEST_ASSERT_TRUE(decision.after.activeManualRun.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(decision.after.processState.state));
}

void test_completion_can_return_to_standby_or_start_manual_cooling() {
    auto completed = startedProgramState();
    completed.processState.state = ProcessState::Completed;
    completed.processState.targetReachStartedAtMillis = 0U;
    completed.processState.targetReachWarningIssued = false;
    CompletionRequest acknowledge{envelope(2U, completed), false, std::nullopt,
                                  false};
    const auto acknowledged = decideCompletion(completed, acknowledge);
    TEST_ASSERT_TRUE(acknowledged.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Standby),
        static_cast<int>(acknowledged.after.processState.state));

    CompletionRequest cool{envelope(3U, completed), true, manualPlan("cool"),
                           true};
    const auto cooling = decideCompletion(completed, cool);
    TEST_ASSERT_TRUE(cooling.proposed());
    TEST_ASSERT_TRUE(cooling.after.activeManualRun.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(cooling.after.processState.state));
}

RunAdjustmentCommandRequest targetChange(const RunCommandState& state,
                                         CommandId id, double target,
                                         std::uint64_t time) {
    RunAdjustmentCommandRequest request;
    request.envelope =
        envelope(id, state, CommandSource::WebInterface, true, time);
    request.targetTemperatureCelsius = target;
    request.safetyAllowsChange = true;
    return request;
}

ApplyRecoveryTimeCorrectionRequest recoveryCorrection(
    const RunCommandState& state, CommandId id, std::uint32_t seconds,
    std::uint64_t time) {
    ApplyRecoveryTimeCorrectionRequest request;
    request.envelope =
        envelope(id, state, CommandSource::LocalDisplay, true, time);
    request.envelope.expectedRecoveryEpisodeRevision =
        state.recoveryEpisodeRevision;
    request.secondsDelta = seconds;
    return request;
}

void test_target_adjustments_requalify_before_fermentation_only() {
    const std::array<ProcessState, 3U> requalifying{{
        ProcessState::Preheating,
        ProcessState::ReachingTarget,
        ProcessState::QualifyingTarget,
    }};
    for (const auto phase : requalifying) {
        auto state = phase == ProcessState::Preheating
                         ? startedProgramState("yogurt-mild")
                         : startedProgramState();
        state.processState.state = phase;
        if (phase != ProcessState::ReachingTarget) {
            state.processState.qualificationValidSinceMillis = 150U;
        }
        const auto decision =
            decideRunAdjustment(state, targetChange(state, 2U, 39.0, 200U));
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_TRUE(
            decision.adjustmentPreview->targetRequalificationRequired);
        TEST_ASSERT_FALSE(decision.after.processState
                              .qualificationValidSinceMillis.has_value());
        if (phase == ProcessState::QualifyingTarget) {
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(ProcessState::ReachingTarget),
                static_cast<int>(decision.after.processState.state));
        }
    }

    auto fermenting = startedProgramState();
    fermenting.processState.state = ProcessState::Fermenting;
    fermenting.processState.stateEnteredAtMillis = 75U;
    const auto biological = decideRunAdjustment(
        fermenting, targetChange(fermenting, 2U, 39.0, 200U));
    TEST_ASSERT_TRUE(biological.proposed());
    TEST_ASSERT_TRUE(biological.adjustmentPreview
                         ->timerContinuesWithoutBiologicalCorrection);
    TEST_ASSERT_FALSE(
        biological.adjustmentPreview->targetRequalificationRequired);
    TEST_ASSERT_EQUAL_UINT64(
        75U, biological.after.processState.stateEnteredAtMillis);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fermenting),
        static_cast<int>(biological.after.processState.state));
}

void test_duration_adjustment_restarts_remaining_timer_and_allows_zero() {
    auto state = startedProgramState();
    state.processState.state = ProcessState::Fermenting;
    state.processState.stateEnteredAtMillis = 50U;
    RunAdjustmentCommandRequest request;
    request.envelope =
        envelope(2U, state, CommandSource::LocalDisplay, true, 300U);
    request.remainingDurationMinutes = 0U;
    request.safetyAllowsChange = true;

    const auto decision = decideRunAdjustment(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_UINT32(0U,
                             decision.after.activeProgramRun->effectiveValues()
                                 .remainingDurationMinutes);
    TEST_ASSERT_EQUAL_UINT64(300U,
                             decision.after.processState.stateEnteredAtMillis);
    TEST_ASSERT_FALSE(
        decision.adjustmentPreview->targetRequalificationRequired);
    TEST_ASSERT_FALSE(
        decision.adjustmentPreview->timerContinuesWithoutBiologicalCorrection);
}

void test_duration_adjustment_folds_observed_time_and_resets_recovery_baseline() {
    auto state = startedProgramState();
    state.processState.state = ProcessState::Fermenting;
    state.processState.stateEnteredAtMillis = 50U;
    state.runProgress.observedRunSeconds = 10U;
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{80U, 120U}};
    state.nominalRecoveryAdjustment =
        NominalRecoveryAdjustmentState{20U, 3U, 20U};
    PendingRecoveryAnchor anchor;
    anchor.originalProcessState.state = ProcessState::Fermenting;
    state.pendingRecoveryAnchor = anchor;
    state.recoveryBootAnchorMonotonicMillis = 25U;

    RunAdjustmentCommandRequest request;
    request.envelope =
        envelope(2U, state, CommandSource::LocalDisplay, true, 5'050U);
    request.remainingDurationMinutes = 60U;
    request.safetyAllowsChange = true;
    const auto decision = decideRunAdjustment(state, request);

    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_UINT32(15U,
                             decision.after.runProgress.observedRunSeconds);
    TEST_ASSERT_EQUAL_UINT64(5'050U,
                             decision.after.processState.stateEnteredAtMillis);
    TEST_ASSERT_TRUE(decision.after.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        0U, decision.after.priorBootPhaseElapsed->elapsed.lowerBoundSeconds);
    TEST_ASSERT_EQUAL_UINT32(
        0U, *decision.after.priorBootPhaseElapsed->elapsed.upperBoundSeconds);
    TEST_ASSERT_FALSE(decision.after.nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_FALSE(decision.after.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_FALSE(
        decision.after.recoveryBootAnchorMonotonicMillis.has_value());

    auto applied = state;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(applied, decision)));
    TEST_ASSERT_EQUAL_UINT32(15U, applied.runProgress.observedRunSeconds);
}

void test_target_only_adjustment_keeps_recovery_and_timer_baseline() {
    auto state = startedProgramState();
    state.processState.state = ProcessState::Fermenting;
    state.processState.stateEnteredAtMillis = 50U;
    state.runProgress.observedRunSeconds = 10U;
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{80U, 120U}};
    state.nominalRecoveryAdjustment =
        NominalRecoveryAdjustmentState{20U, 3U, 20U};
    PendingRecoveryAnchor anchor;
    anchor.originalProcessState.state = ProcessState::Fermenting;
    state.pendingRecoveryAnchor = anchor;
    state.recoveryBootAnchorMonotonicMillis = 25U;

    const auto decision =
        decideRunAdjustment(state, targetChange(state, 2U, 39.0, 5'050U));
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_UINT32(10U,
                             decision.after.runProgress.observedRunSeconds);
    TEST_ASSERT_EQUAL_UINT64(50U,
                             decision.after.processState.stateEnteredAtMillis);
    TEST_ASSERT_TRUE(decision.after.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        80U, decision.after.priorBootPhaseElapsed->elapsed.lowerBoundSeconds);
    TEST_ASSERT_EQUAL_UINT32(
        120U, *decision.after.priorBootPhaseElapsed->elapsed.upperBoundSeconds);
    TEST_ASSERT_TRUE(decision.after.nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_TRUE(decision.after.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_EQUAL_UINT64(25U,
                             *decision.after.recoveryBootAnchorMonotonicMillis);
}

void test_recovery_time_correction_is_cumulative_bounded_and_idempotent() {
    auto state = startedProgramState();
    state.processState.state = ProcessState::Fermenting;
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{100U, 300U}};
    state.recoveryEpisodeRevision = 4U;

    const auto first = recoveryCorrection(state, 10U, 50U, 100U);
    const auto firstDecision = decideApplyRecoveryTimeCorrection(state, first);
    TEST_ASSERT_TRUE(firstDecision.proposed());
    TEST_ASSERT_EQUAL_UINT32(50U, firstDecision.after.nominalRecoveryAdjustment
                                      ->cumulativeAppliedSeconds);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::Applied),
        static_cast<int>(applyRunCommand(state, firstDecision)));

    const auto duplicate = recoveryCorrection(state, 11U, 50U, 101U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::AlreadyProcessed),
        static_cast<int>(
            decideApplyRecoveryTimeCorrection(state, duplicate).status));
    const auto conflicting = recoveryCorrection(state, 12U, 51U, 102U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::NotAllowedInState),
        static_cast<int>(
            decideApplyRecoveryTimeCorrection(state, conflicting).status));

    state.recoveryEpisodeRevision = 5U;
    const auto second = recoveryCorrection(state, 13U, 100U, 103U);
    const auto secondDecision =
        decideApplyRecoveryTimeCorrection(state, second);
    TEST_ASSERT_TRUE(secondDecision.proposed());
    TEST_ASSERT_EQUAL_UINT32(150U,
                             secondDecision.after.nominalRecoveryAdjustment
                                 ->cumulativeAppliedSeconds);

    const auto outOfBounds = recoveryCorrection(state, 14U, 151U, 104U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::InvalidInput),
        static_cast<int>(
            decideApplyRecoveryTimeCorrection(state, outOfBounds).status));

    auto unknown = state;
    unknown.priorBootPhaseElapsed->elapsed.upperBoundSeconds.reset();
    const auto unknownBounds = recoveryCorrection(unknown, 15U, 1U, 105U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::NotAllowedInState),
        static_cast<int>(
            decideApplyRecoveryTimeCorrection(unknown, unknownBounds).status));
}

void test_nominal_recovery_correction_does_not_change_r1_phase_timer() {
    auto state = startedProgramState();
    state.processState.state = ProcessState::Fermenting;
    state.processState.stateEnteredAtMillis = 1'000U;
    state.processState.targetReachStartedAtMillis = 0U;
    state.processState.targetReachWarningIssued = false;
    state.processState.qualificationValidSinceMillis.reset();
    state.processRunSnapshot->fermentationDurationMinutes = 1U;
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{30U, 60U}};
    state.nominalRecoveryAdjustment =
        NominalRecoveryAdjustmentState{30U, 4U, 30U};

    const auto effective = effectivePriorElapsedForFermenting(state);
    TEST_ASSERT_TRUE(effective.has_value());
    TEST_ASSERT_EQUAL_UINT32(30U, effective->lowerBoundSeconds);
    TEST_ASSERT_EQUAL_UINT32(60U, *effective->upperBoundSeconds);

    const auto decision = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{}, 1'000U, *effective);
    TEST_ASSERT_FALSE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecisionStatus::NoTransition),
                          static_cast<int>(decision.status));
}

void test_late_run_adjustment_rejections_discard_the_complete_candidate() {
    // Die ActiveRun-Aenderung ist gueltig, aber der anschliessende
    // TargetChanged-Uebergang lehnt die ruecklaeufige Prozesszeit ab.
    {
        auto state = startedProgramState();
        state.processState.stateEnteredAtMillis = 300U;
        const auto decision =
            decideRunAdjustment(state, targetChange(state, 2U, 39.0, 200U));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
        TEST_ASSERT_FALSE(decision.adjustmentPreview.has_value());
        TEST_ASSERT_EQUAL_DOUBLE(
            state.activeProgramRun->effectiveValues().targetTemperatureCelsius,
            decision.after.activeProgramRun->effectiveValues()
                .targetTemperatureCelsius);
        TEST_ASSERT_EQUAL_UINT32(
            state.activeProgramRun->revisionCount(),
            decision.after.activeProgramRun->revisionCount());
        TEST_ASSERT_EQUAL_UINT32(state.runRevision, decision.after.runRevision);
    }

    // Derselbe spaete Ablehnungspfad bei erschoepfter Prozesssequenz.
    {
        auto state = startedProgramState();
        state.processState.transitionSequence =
            std::numeric_limits<std::uint32_t>::max();
        const auto decision =
            decideRunAdjustment(state, targetChange(state, 2U, 39.0, 200U));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
        TEST_ASSERT_FALSE(decision.adjustmentPreview.has_value());
        TEST_ASSERT_EQUAL_DOUBLE(
            state.activeProgramRun->effectiveValues().targetTemperatureCelsius,
            decision.after.activeProgramRun->effectiveValues()
                .targetTemperatureCelsius);
        TEST_ASSERT_EQUAL_UINT32(
            state.activeProgramRun->revisionCount(),
            decision.after.activeProgramRun->revisionCount());
        TEST_ASSERT_EQUAL_UINT32(state.runRevision, decision.after.runRevision);
    }
}

void test_composed_cooling_rejections_discard_the_complete_candidate() {
    const auto almostFull = std::numeric_limits<std::uint32_t>::max() - 1U;

    // Abort gelingt mit der letzten Prozesssequenz; der anschliessende
    // manuelle Kuehlstart muss scheitern, ohne den Alt-Lauf zu entfernen.
    {
        auto state = startedProgramState();
        state.processState.transitionSequence = almostFull;
        StopRequest request{envelope(2U, state), StopOption::AbortAndCool,
                            manualPlan("cool"), true};
        const auto decision = decideStop(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
        TEST_ASSERT_TRUE(decision.after.activeProgramRun.has_value());
        TEST_ASSERT_FALSE(decision.after.activeManualRun.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(state.processState.state),
            static_cast<int>(decision.after.processState.state));
        TEST_ASSERT_TRUE(decision.after.processRunSnapshot.has_value());
    }

    // Abschlussquittierung gelingt ebenfalls noch; der zweite Uebergang darf
    // den abgeschlossenen Ursprungslauf bei Ablehnung nicht teilweise loeschen.
    {
        auto completed = startedProgramState();
        completed.processState.state = ProcessState::Completed;
        completed.processState.targetReachStartedAtMillis = 0U;
        completed.processState.targetReachWarningIssued = false;
        completed.processState.transitionSequence = almostFull;
        CompletionRequest request{envelope(2U, completed), true,
                                  manualPlan("cool"), true};
        const auto decision = decideCompletion(completed, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ProcessState::Completed),
            static_cast<int>(decision.after.processState.state));
        TEST_ASSERT_TRUE(decision.after.activeProgramRun.has_value());
        TEST_ASSERT_FALSE(decision.after.activeManualRun.has_value());
        TEST_ASSERT_TRUE(decision.after.processRunSnapshot.has_value());
    }
}

void test_adjustments_are_rejected_in_inappropriate_states() {
    const std::array<ProcessState, 8U> states{{
        ProcessState::Cooling,
        ProcessState::CoolHolding,
        ProcessState::ManualHolding,
        ProcessState::Completed,
        ProcessState::Fault,
        ProcessState::SafeBoot,
        ProcessState::RecoveryEvaluation,
        ProcessState::ServiceMode,
    }};
    for (const auto phase : states) {
        auto state = startedProgramState();
        state.processState.state = phase;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(CommandStatus::NotAllowedInState),
            static_cast<int>(
                decideRunAdjustment(state, targetChange(state, 2U, 39.0, 200U))
                    .status));
    }
}

void test_message_priority_acknowledgement_and_mute_are_independent() {
    auto state = standbyState();
    state.messageCount = 2U;
    state.messages[0] =
        message(1U, MessageCode::UserDecisionRequired,
                MessageClass::DecisionRequired, 10U, MessageTrigger::Process);
    state.messages[1] =
        message(2U, MessageCode::SafetyFault, MessageClass::SafetyFault, 1U,
                MessageTrigger::Safety);
    state.criticalSafetyEventPending = true;
    TEST_ASSERT_EQUAL_UINT32(2U, highestPriorityActiveMessage(state)->id);

    MessageCommandRequest mute{envelope(10U, state), 2U};
    const auto muted = decideMuteMessage(state, mute);
    TEST_ASSERT_TRUE(muted.proposed());
    TEST_ASSERT_TRUE(muted.after.messages[1].acousticMuted);
    TEST_ASSERT_FALSE(muted.after.messages[1].acknowledged);
    TEST_ASSERT_TRUE(muted.after.criticalSafetyEventPending);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(state, muted)));

    MessageCommandRequest acknowledge{envelope(11U, state), 2U};
    const auto acknowledged = decideAcknowledgeMessage(state, acknowledge);
    TEST_ASSERT_TRUE(acknowledged.proposed());
    TEST_ASSERT_TRUE(acknowledged.after.messages[1].acknowledged);
    TEST_ASSERT_TRUE(acknowledged.after.messages[1].acousticMuted);
    TEST_ASSERT_TRUE(acknowledged.after.criticalSafetyEventPending);
}

void test_fault_reset_requires_current_qualified_evaluation() {
    auto state = standbyState();
    state.processState.state = ProcessState::Fault;
    state.faultRevision = 4U;
    state.criticalSafetyEventPending = true;
    FaultResetRequest request;
    request.envelope = envelope(1U, state);
    request.evaluation = {
        true, false, true, true, false, 4U, FaultResetRejection::None};

    const auto accepted = decideFaultReset(state, request);
    TEST_ASSERT_TRUE(accepted.proposed());
    TEST_ASSERT_FALSE(accepted.after.criticalSafetyEventPending);
    TEST_ASSERT_EQUAL_UINT32(5U, accepted.after.faultRevision);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                          static_cast<int>(accepted.after.processState.state));
    TEST_ASSERT_TRUE(hasEffect(accepted, CommandEffect::FaultResetAuthorized));

    request.evaluation.causeStillActive = true;
    request.evaluation.allowed = false;
    request.evaluation.rejection = FaultResetRejection::CauseStillActive;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::SafetyRejected),
        static_cast<int>(decideFaultReset(state, request).status));
    request.evaluation.faultRevision = 3U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::StaleState),
        static_cast<int>(decideFaultReset(state, request).status));
}

void test_critical_safety_blocks_run_commands_but_not_message_commands() {
    auto state = standbyState();
    state.criticalSafetyEventPending = true;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::SafetyRejected),
        static_cast<int>(
            decideProgramStart(state, programStart(state, 1U)).status));

    state.messageCount = 1U;
    state.messages[0] =
        message(7U, MessageCode::SafetyFault, MessageClass::SafetyFault, 1U,
                MessageTrigger::Safety);
    MessageCommandRequest message{envelope(2U, state), 7U};
    TEST_ASSERT_TRUE(decideAcknowledgeMessage(state, message).proposed());
    TEST_ASSERT_TRUE(decideMuteMessage(state, message).proposed());
}

void test_critical_safety_event_invalidates_a_pending_comfort_decision() {
    auto state = standbyState();
    const auto pendingStart =
        decideProgramStart(state, programStart(state, 10U));
    TEST_ASSERT_TRUE(pendingStart.proposed());

    state.criticalSafetyEventPending = true;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::SafetyRejected),
        static_cast<int>(applyRunCommand(state, pendingStart)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_FALSE(state.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, state.commandSequence);
    TEST_ASSERT_EQUAL_UINT32(0U, state.processedCommandCount);
}

void test_domain_revision_conflicts_are_rejected_without_mutation() {
    auto runState = startedProgramState();
    auto adjustment = targetChange(runState, 2U, 39.0, 200U);
    adjustment.envelope.expectedRunRevision = runState.runRevision - 1U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::StaleState),
        static_cast<int>(decideRunAdjustment(runState, adjustment).status));
    TEST_ASSERT_EQUAL_DOUBLE(
        38.0,
        runState.activeProgramRun->effectiveValues().targetTemperatureCelsius);

    auto messageState = standbyState();
    messageState.messageCount = 1U;
    messageState.messageRevision = 3U;
    messageState.messages[0] =
        message(7U, MessageCode::RunCompleted, MessageClass::Information, 1U,
                MessageTrigger::Process);
    auto messageRequest = MessageCommandRequest{envelope(3U, messageState), 7U};
    messageRequest.envelope.expectedMessageRevision = 2U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::StaleState),
        static_cast<int>(
            decideAcknowledgeMessage(messageState, messageRequest).status));
    TEST_ASSERT_FALSE(messageState.messages[0].acknowledged);

    auto faultState = standbyState();
    faultState.processState.state = ProcessState::Fault;
    faultState.faultRevision = 4U;
    FaultResetRequest reset;
    reset.envelope = envelope(4U, faultState);
    reset.envelope.expectedFaultRevision = 3U;
    reset.evaluation = {
        true, false, true, true, false, 4U, FaultResetRejection::None};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::StaleState),
        static_cast<int>(decideFaultReset(faultState, reset).status));
    TEST_ASSERT_EQUAL_UINT32(4U, faultState.faultRevision);
}

void test_processed_command_ids_form_a_bounded_rolling_window() {
    auto state = standbyState();
    CommandId commandId = 1U;
    for (std::size_t cycle = 0U;
         cycle < run_command_limits::kMaximumProcessedCommandIds; ++cycle) {
        auto start = programStart(state, commandId);
        const auto startDecision = decideProgramStart(state, start);
        TEST_ASSERT_TRUE(startDecision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(CommandStatus::Applied),
            static_cast<int>(applyRunCommand(state, startDecision)));
        ++commandId;

        StopRequest stop{envelope(commandId, state),
                         StopOption::AbortAndTurnOff, std::nullopt, false};
        const auto stopDecision = decideStop(state, stop);
        TEST_ASSERT_TRUE(stopDecision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(CommandStatus::Applied),
            static_cast<int>(applyRunCommand(state, stopDecision)));
        ++commandId;
    }

    TEST_ASSERT_EQUAL_UINT32(run_command_limits::kMaximumProcessedCommandIds,
                             state.processedCommandCount);
    const auto recentId = commandId - 1U;
    StopRequest duplicate{envelope(recentId, state),
                          StopOption::AbortAndTurnOff, std::nullopt, false};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandStatus::AlreadyProcessed),
        static_cast<int>(decideStop(state, duplicate).status));

    const auto next = decideProgramStart(state, programStart(state, commandId));
    TEST_ASSERT_TRUE(next.proposed());
}

void test_run_revision_overflow_is_rejected_for_every_run_mutating_command() {
    const std::uint32_t max = std::numeric_limits<std::uint32_t>::max();

    // Programmstart: an der Kapazitaetsgrenze abgelehnt, davor genau einmal
    // erhoehbar.
    {
        auto state = standbyState();
        state.runRevision = max;
        const auto decision =
            decideProgramStart(state, programStart(state, 1U));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.startSummary.has_value());
        TEST_ASSERT_FALSE(decision.after.activeProgramRun.has_value());
        TEST_ASSERT_EQUAL_UINT32(max, decision.after.runRevision);
        assertRejectedWithoutStateMutation(decision);

        state.runRevision = max - 1U;
        const auto ok = decideProgramStart(state, programStart(state, 1U));
        TEST_ASSERT_TRUE(ok.proposed());
        TEST_ASSERT_EQUAL_UINT32(max, ok.after.runRevision);
    }
    // Manueller Start: gleiche Grenze.
    {
        auto state = standbyState();
        state.runRevision = max;
        auto request = manualStart(state, 1U, manualPlan("hold"));
        const auto decision = decideManualStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.startSummary.has_value());
        TEST_ASSERT_FALSE(decision.after.activeManualRun.has_value());
        assertRejectedWithoutStateMutation(decision);

        state.runRevision = max - 1U;
        auto okRequest = manualStart(state, 1U, manualPlan("hold"));
        const auto ok = decideManualStart(state, okRequest);
        TEST_ASSERT_TRUE(ok.proposed());
        TEST_ASSERT_EQUAL_UINT32(max, ok.after.runRevision);
    }
    // Abbrechen und ausschalten.
    {
        auto state = startedProgramState();
        state.runRevision = max;
        StopRequest request{envelope(2U, state), StopOption::AbortAndTurnOff,
                            std::nullopt, false};
        const auto decision = decideStop(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.after.activeProgramRun.has_value());
        assertRejectedWithoutStateMutation(decision);
    }
    // Abbrechen und kuehlen.
    {
        auto state = startedProgramState();
        state.runRevision = max;
        StopRequest request{envelope(2U, state), StopOption::AbortAndCool,
                            manualPlan("cool"), true};
        const auto decision = decideStop(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.after.activeProgramRun.has_value());
        TEST_ASSERT_FALSE(decision.after.activeManualRun.has_value());
        assertRejectedWithoutStateMutation(decision);
    }
    // Abschluss quittieren.
    {
        auto completed = startedProgramState();
        completed.processState.state = ProcessState::Completed;
        completed.processState.targetReachStartedAtMillis = 0U;
        completed.processState.targetReachWarningIssued = false;
        completed.runRevision = max;
        CompletionRequest request{envelope(2U, completed), false, std::nullopt,
                                  false};
        const auto decision = decideCompletion(completed, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ProcessState::Completed),
            static_cast<int>(decision.after.processState.state));
        assertRejectedWithoutStateMutation(decision);
    }
    // Kuehlen nach Abschluss.
    {
        auto completed = startedProgramState();
        completed.processState.state = ProcessState::Completed;
        completed.processState.targetReachStartedAtMillis = 0U;
        completed.processState.targetReachWarningIssued = false;
        completed.runRevision = max;
        CompletionRequest request{envelope(3U, completed), true,
                                  manualPlan("cool"), true};
        const auto decision = decideCompletion(completed, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.after.activeManualRun.has_value());
        assertRejectedWithoutStateMutation(decision);
    }
    // Laufanpassung: an der Grenze abgelehnt, davor genau einmal erhoehbar.
    {
        auto state = startedProgramState();
        state.runRevision = max;
        const auto decision =
            decideRunAdjustment(state, targetChange(state, 2U, 39.0, 200U));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_EQUAL_DOUBLE(
            38.0, decision.after.activeProgramRun->effectiveValues()
                      .targetTemperatureCelsius);
        assertRejectedWithoutStateMutation(decision);

        state.runRevision = max - 1U;
        const auto ok =
            decideRunAdjustment(state, targetChange(state, 2U, 39.0, 200U));
        TEST_ASSERT_TRUE(ok.proposed());
        TEST_ASSERT_EQUAL_UINT32(max, ok.after.runRevision);
    }
}

void test_run_revision_capacity_does_not_mask_prior_domain_results() {
    const std::uint32_t max = std::numeric_limits<std::uint32_t>::max();

    // Ein gueltiger Start liefert auch an der Revisionsgrenze zuerst die
    // Vorschau/Bestaetigungsantwort; ungueltige Daten behalten ihre Diagnose.
    {
        auto state = standbyState();
        state.runRevision = max;
        auto request = programStart(state, 1U);
        request.envelope.confirmed = false;
        const auto unconfirmed = decideProgramStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                              static_cast<int>(unconfirmed.status));
        TEST_ASSERT_TRUE(unconfirmed.startSummary.has_value());
        assertRejectedWithoutStateMutation(unconfirmed);

        request.sourceProgramRevision = 0U;
        const auto invalid = decideProgramStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(invalid.status));
        assertRejectedWithoutStateMutation(invalid);

        request.sourceProgramRevision = 1U;
        request.safetyAllowsStart = false;
        const auto unsafe = decideProgramStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                              static_cast<int>(unsafe.status));
        assertRejectedWithoutStateMutation(unsafe);
    }
    {
        auto state = standbyState();
        state.runRevision = max;
        auto request = manualStart(state, 1U, manualPlan("hold"));
        request.envelope.confirmed = false;
        const auto decision = decideManualStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.startSummary.has_value());
        assertRejectedWithoutStateMutation(decision);
    }
    // Stop und Abschluss pruefen Bestaetigung und Zustand vor der Kapazitaet.
    {
        auto state = startedProgramState();
        state.runRevision = max;
        StopRequest request{envelope(2U, state), StopOption::AbortAndTurnOff,
                            std::nullopt, false};
        request.envelope.confirmed = false;
        const auto unconfirmed = decideStop(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                              static_cast<int>(unconfirmed.status));
        assertRejectedWithoutStateMutation(unconfirmed);

        auto standby = standbyState();
        standby.runRevision = max;
        request.envelope = envelope(2U, standby);
        const auto disallowed = decideStop(standby, request);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(CommandStatus::NotAllowedInState),
            static_cast<int>(disallowed.status));
        assertRejectedWithoutStateMutation(disallowed);
    }
    {
        auto completed = startedProgramState();
        completed.processState.state = ProcessState::Completed;
        completed.processState.targetReachStartedAtMillis = 0U;
        completed.processState.targetReachWarningIssued = false;
        completed.runRevision = max;
        CompletionRequest request{envelope(2U, completed), false, std::nullopt,
                                  false};
        request.envelope.confirmed = false;
        const auto unconfirmed = decideCompletion(completed, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                              static_cast<int>(unconfirmed.status));
        assertRejectedWithoutStateMutation(unconfirmed);

        auto standby = standbyState();
        standby.runRevision = max;
        request.envelope = envelope(2U, standby);
        const auto decision = decideCompletion(standby, request);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(CommandStatus::NotAllowedInState),
            static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }
    // Laufanpassungen behalten NotConfirmed, NoChange und InvalidInput.
    {
        auto state = startedProgramState();
        state.runRevision = max;
        auto request = targetChange(state, 2U, 39.0, 200U);
        request.envelope.confirmed = false;
        const auto unconfirmed = decideRunAdjustment(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NotConfirmed),
                              static_cast<int>(unconfirmed.status));
        assertRejectedWithoutStateMutation(unconfirmed);

        request.envelope.confirmed = true;
        request.targetTemperatureCelsius = 38.0;
        const auto noChange = decideRunAdjustment(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NoChange),
                              static_cast<int>(noChange.status));
        assertRejectedWithoutStateMutation(noChange);

        request.targetTemperatureCelsius = 100.0;
        const auto invalid = decideRunAdjustment(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(invalid.status));
        assertRejectedWithoutStateMutation(invalid);
    }
}

void test_message_and_fault_revision_overflow_is_rejected() {
    const std::uint32_t max = std::numeric_limits<std::uint32_t>::max();

    // Einzelne Meldungsrevision an der Grenze: quittieren abgelehnt.
    {
        auto state = standbyState();
        state.messageCount = 1U;
        state.messages[0] =
            message(7U, MessageCode::RunCompleted, MessageClass::Information,
                    1U, MessageTrigger::Process);
        state.messages[0].revision = max;
        MessageCommandRequest request{envelope(1U, state), 7U};
        const auto decision = decideAcknowledgeMessage(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.after.messages[0].acknowledged);
        assertRejectedWithoutStateMutation(decision);
    }
    // Fachrevision (aggregiert) an der Grenze, einzelne Meldungsrevision
    // unauffaellig: ebenfalls abgelehnt.
    {
        auto state = standbyState();
        state.messageRevision = max;
        state.messageCount = 1U;
        state.messages[0] =
            message(7U, MessageCode::RunCompleted, MessageClass::Information,
                    1U, MessageTrigger::Process);
        MessageCommandRequest request{envelope(1U, state), 7U};
        const auto decision = decideAcknowledgeMessage(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.after.messages[0].acknowledged);
        assertRejectedWithoutStateMutation(decision);
    }
    // Dieselben zwei Faelle fuer Stummschalten.
    {
        auto state = standbyState();
        state.messageCount = 1U;
        state.messages[0] =
            message(8U, MessageCode::RunCompleted, MessageClass::Information,
                    1U, MessageTrigger::Process);
        state.messages[0].revision = max;
        MessageCommandRequest request{envelope(1U, state), 8U};
        const auto decision = decideMuteMessage(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.after.messages[0].acousticMuted);
        assertRejectedWithoutStateMutation(decision);
    }
    {
        auto state = standbyState();
        state.messageRevision = max;
        state.messageCount = 1U;
        state.messages[0] =
            message(8U, MessageCode::RunCompleted, MessageClass::Information,
                    1U, MessageTrigger::Process);
        MessageCommandRequest request{envelope(1U, state), 8U};
        const auto decision = decideMuteMessage(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_FALSE(decision.after.messages[0].acousticMuted);
        assertRejectedWithoutStateMutation(decision);
    }
    // Fehlerrevision an der Grenze abgelehnt, davor genau einmal erhoehbar.
    {
        auto state = standbyState();
        state.processState.state = ProcessState::Fault;
        state.faultRevision = max;
        state.criticalSafetyEventPending = true;
        FaultResetRequest request;
        request.envelope = envelope(1U, state);
        request.evaluation = {
            true, false, true, true, false, max, FaultResetRejection::None};
        const auto decision = decideFaultReset(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.after.criticalSafetyEventPending);
        TEST_ASSERT_EQUAL_UINT32(max, decision.after.faultRevision);
        assertRejectedWithoutStateMutation(decision);

        state.faultRevision = max - 1U;
        FaultResetRequest okRequest;
        okRequest.envelope = envelope(2U, state);
        okRequest.evaluation = {true,
                                false,
                                true,
                                true,
                                false,
                                max - 1U,
                                FaultResetRejection::None};
        const auto ok = decideFaultReset(state, okRequest);
        TEST_ASSERT_TRUE(ok.proposed());
        TEST_ASSERT_EQUAL_UINT32(max, ok.after.faultRevision);
    }
}

// ---------------------------------------------------------------------------
// #21, 6.14.3: Kommandovertrag fuer manuelle Sensorentscheidungen (Commit 4)
// ---------------------------------------------------------------------------

SensorQualitySnapshot snapshotWithQuality(SensorQuality quality) {
    SensorQualitySnapshot snapshot;
    snapshot.quality = quality;
    if (quality != SensorQuality::Valid) {
        snapshot.lastFaultReason = SensorFaultReason::MissingSample;
    }
    return snapshot;
}

SensorQualitySnapshot validSnapshot() {
    return snapshotWithQuality(SensorQuality::Valid);
}

SensorQualitySnapshot failedSnapshot() {
    return snapshotWithQuality(SensorQuality::Failed);
}

CrossRolePlausibilityContext plausibilityWith(SensorQualitySnapshot air,
                                              SensorQualitySnapshot product,
                                              SensorQualitySnapshot cooling) {
    CrossRolePlausibilityContext ctx;
    ctx.air = std::move(air);
    ctx.product = std::move(product);
    ctx.cooling = std::move(cooling);
    ctx.evaluationMonotonicMillis = 1'000'000U;
    return ctx;
}

RunCommandState startedProgramStateWithReturnStrategy(ReturnStrategy strategy) {
    auto state = standbyState();
    auto request = programStart(state, 1U);
    request.program.program.productSensorFailure.returnStrategy = strategy;
    const auto decision = decideProgramStart(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(state, decision)));
    return state;
}

RunCommandState startedManualProductState() {
    auto state = standbyState();
    auto plan = manualPlan("manual-product-1");
    plan.sensorMode = RunSensorMode::Product;
    auto request = manualStart(state, 1U, plan);
    const auto decision = decideManualStart(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(state, decision)));
    return state;
}

// Setzt eine gewuenschte Sensorselektionsphase auf einem bereits gestarteten
// Lauf (Programm oder manuell). Der Startpfad selbst befuellt
// sensorSelectionRuntime/sensorSelection seit #21 Commit 5 bereits beim
// Start; diese Hilfsfunktion wird trotzdem gebraucht, um gezielt eine
// beliebige Zwischenphase (z. B. UserDecisionRequired/Blocked) fuer
// Kommandovertragstests herzustellen, die eine echte Startsequenz so nicht
// erreichen wuerde.
RunCommandState withSensorPhase(RunCommandState base,
                                SensorSelectionPhase phase,
                                SensorPeltierPermission permission,
                                RunSensorMode activeMode) {
    base.sensorSelectionRuntime.phase = phase;
    base.sensorSelectionRuntime.permission = permission;
    base.activeRunSensorMode = activeMode;
    if (base.activeManualRun.has_value()) {
        base.activeManualRun->values.sensorMode = activeMode;
    }
    base.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, base.runRevision};
    return base;
}

SensorSelectionCommandRequest sensorSelectionRequest(
    const RunCommandState& state, CommandId id,
    SensorSelectionUserAction action, bool safetyAllowsChange = true) {
    SensorSelectionCommandRequest request;
    request.envelope = envelope(id, state);
    request.action = action;
    request.safetyAllowsChange = safetyAllowsChange;
    return request;
}

void test_sensor_selection_action_continue_with_air_valid_and_invalid_states() {
    // Gueltig: UserDecisionRequired, Produkt ausgefallen, Luft/Kuehlung
    // gueltig - Blocked -> Allowed loest den Restored-Effekt aus (keine
    // direkte Aktorfreigabe, nur die #21-Vorbedingung).
    auto blocked = withSensorPhase(startedProgramStateWithReturnStrategy(
                                       ReturnStrategy::ManualReturnToProduct),
                                   SensorSelectionPhase::UserDecisionRequired,
                                   SensorPeltierPermission::Blocked,
                                   RunSensorMode::Product);
    const auto request = sensorSelectionRequest(
        blocked, 50U, SensorSelectionUserAction::ContinueWithAir);
    const auto plausibility =
        plausibilityWith(validSnapshot(), failedSnapshot(), validSnapshot());
    const auto decision =
        decideApplySensorSelectionAction(blocked, request, plausibility);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.sensorSelectionApplyStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SensorSelectionApplyStatus::AppliedPersistentCandidate),
        static_cast<int>(*decision.sensorSelectionApplyStatus));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::AirFallbackActive),
        static_cast<int>(decision.after.sensorSelectionRuntime.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Allowed),
        static_cast<int>(decision.after.sensorSelectionRuntime.permission));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Air),
        static_cast<int>(*decision.after.activeRunSensorMode));
    TEST_ASSERT_TRUE(decision.sensorSelectionEvent.has_value());
    TEST_ASSERT_FALSE(decision.sensorSelectionNotice.has_value());
    TEST_ASSERT_TRUE(
        hasEffect(decision, CommandEffect::SensorSelectionPermissionRestored));
    TEST_ASSERT_FALSE(
        hasEffect(decision, CommandEffect::SensorSelectionPermissionBlocked));
    TEST_ASSERT_EQUAL_UINT32(blocked.runRevision + 1U,
                             decision.after.runRevision);

    // Ungueltig: NormalProduct nimmt keine Benutzeraktion an.
    auto normal = withSensorPhase(startedProgramStateWithReturnStrategy(
                                      ReturnStrategy::ManualReturnToProduct),
                                  SensorSelectionPhase::NormalProduct,
                                  SensorPeltierPermission::Allowed,
                                  RunSensorMode::Product);
    const auto invalidRequest = sensorSelectionRequest(
        normal, 51U, SensorSelectionUserAction::ContinueWithAir);
    const auto invalidDecision =
        decideApplySensorSelectionAction(normal, invalidRequest, plausibility);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                          static_cast<int>(invalidDecision.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionApplyStatus::InvalidDecision),
        static_cast<int>(*invalidDecision.sensorSelectionApplyStatus));
    assertRejectedWithoutStateMutation(invalidDecision);
}

void test_sensor_selection_action_return_to_product_valid_and_invalid_states() {
    // Gueltig: AirFallbackActive, ManualReturnToProduct, Produkt wieder
    // gueltig. Permission bleibt Allowed -> kein Effekt (schon vorher
    // freigegeben, kein Uebergang).
    auto fallback =
        withSensorPhase(startedProgramStateWithReturnStrategy(
                            ReturnStrategy::ManualReturnToProduct),
                        SensorSelectionPhase::AirFallbackActive,
                        SensorPeltierPermission::Allowed, RunSensorMode::Air);
    const auto request = sensorSelectionRequest(
        fallback, 60U, SensorSelectionUserAction::ReturnToProduct);
    const auto plausibility =
        plausibilityWith(validSnapshot(), validSnapshot(), validSnapshot());
    const auto decision =
        decideApplySensorSelectionAction(fallback, request, plausibility);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SensorSelectionApplyStatus::AppliedPersistentCandidate),
        static_cast<int>(*decision.sensorSelectionApplyStatus));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::NormalProduct),
        static_cast<int>(decision.after.sensorSelectionRuntime.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Product),
        static_cast<int>(*decision.after.activeRunSensorMode));
    TEST_ASSERT_TRUE(decision.sensorSelectionEvent.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, decision.effectCount);

    // Ungueltig: NormalProduct nimmt keine Benutzeraktion an.
    auto normal = withSensorPhase(startedProgramStateWithReturnStrategy(
                                      ReturnStrategy::ManualReturnToProduct),
                                  SensorSelectionPhase::NormalProduct,
                                  SensorPeltierPermission::Allowed,
                                  RunSensorMode::Product);
    const auto invalidRequest = sensorSelectionRequest(
        normal, 61U, SensorSelectionUserAction::ReturnToProduct);
    const auto invalidDecision =
        decideApplySensorSelectionAction(normal, invalidRequest, plausibility);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                          static_cast<int>(invalidDecision.status));
    assertRejectedWithoutStateMutation(invalidDecision);
}

void test_sensor_selection_action_recheck_product_ram_only_transport_and_idempotency() {
    auto fallback =
        withSensorPhase(startedProgramStateWithReturnStrategy(
                            ReturnStrategy::AutomaticValidatedReturnToProduct),
                        SensorSelectionPhase::AirFallbackActive,
                        SensorPeltierPermission::Allowed, RunSensorMode::Air);
    // Unvollstaendige Rueckkehrevidenz (thermalCompatibility bleibt
    // Unavailable) - 6.4.12-Sonderfall, RAM-only.
    const auto plausibility =
        plausibilityWith(validSnapshot(), validSnapshot(), validSnapshot());
    const auto request = sensorSelectionRequest(
        fallback, 70U, SensorSelectionUserAction::RecheckProduct);
    const auto decision =
        decideApplySensorSelectionAction(fallback, request, plausibility);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionApplyStatus::AppliedRamOnly),
        static_cast<int>(*decision.sensorSelectionApplyStatus));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::ReturnValidationPending),
        static_cast<int>(decision.after.sensorSelectionRuntime.phase));
    // AppliedRamOnly darf keinen Store-Write ausloesen: Laufrevision und
    // persistierte Provenienz bleiben unveraendert.
    TEST_ASSERT_EQUAL_UINT32(fallback.runRevision, decision.after.runRevision);
    TEST_ASSERT_TRUE(decision.after.sensorSelection.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionDecisionCause::StartSelection),
        static_cast<int>(decision.after.sensorSelection->lastDecisionCause));

    // Stale-gepruefte, fluechtige Anwendung ohne Store-Write.
    auto current = fallback;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(current, decision)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::ReturnValidationPending),
        static_cast<int>(current.sensorSelectionRuntime.phase));

    // Wiederholung derselben CommandId innerhalb desselben Boots: keine
    // zweite RAM-Mutation.
    const auto repeat =
        decideApplySensorSelectionAction(current, request, plausibility);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::AlreadyProcessed),
                          static_cast<int>(repeat.status));
}

void test_sensor_selection_action_no_change_leaves_everything_untouched() {
    auto fallback =
        withSensorPhase(startedProgramStateWithReturnStrategy(
                            ReturnStrategy::AutomaticValidatedReturnToProduct),
                        SensorSelectionPhase::AirFallbackActive,
                        SensorPeltierPermission::Allowed, RunSensorMode::Air);
    // Produkt weiterhin ungueltig -> RecheckProduct bleibt ohne Wirkung.
    const auto plausibility =
        plausibilityWith(validSnapshot(), failedSnapshot(), validSnapshot());
    const auto request = sensorSelectionRequest(
        fallback, 80U, SensorSelectionUserAction::RecheckProduct);
    const auto decision =
        decideApplySensorSelectionAction(fallback, request, plausibility);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NoChange),
                          static_cast<int>(decision.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionApplyStatus::NoChange),
        static_cast<int>(*decision.sensorSelectionApplyStatus));
    assertRejectedWithoutStateMutation(decision);
}

void test_sensor_selection_action_safety_pending_matrix() {
    const auto plausibility =
        plausibilityWith(validSnapshot(), validSnapshot(), validSnapshot());

    // ContinueWithAir/ReturnToProduct enden fail-closed vor jeder Mutation,
    // unabhaengig vom externen safetyAllowsChange-Signal.
    for (const auto action : {SensorSelectionUserAction::ContinueWithAir,
                              SensorSelectionUserAction::ReturnToProduct}) {
        auto state =
            withSensorPhase(startedProgramStateWithReturnStrategy(
                                ReturnStrategy::ManualReturnToProduct),
                            action == SensorSelectionUserAction::ContinueWithAir
                                ? SensorSelectionPhase::UserDecisionRequired
                                : SensorSelectionPhase::AirFallbackActive,
                            action == SensorSelectionUserAction::ContinueWithAir
                                ? SensorPeltierPermission::Blocked
                                : SensorPeltierPermission::Allowed,
                            action == SensorSelectionUserAction::ContinueWithAir
                                ? RunSensorMode::Product
                                : RunSensorMode::Air);
        state.criticalSafetyEventPending = true;
        const auto request = sensorSelectionRequest(state, 90U, action, true);
        const auto decision =
            decideApplySensorSelectionAction(state, request, plausibility);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }

    // RecheckProduct darf pruefen, aber eine daraus entstehende Mutation
    // bleibt bei offenem kritischem Safety-Ereignis verworfen.
    {
        auto state = withSensorPhase(
            startedProgramStateWithReturnStrategy(
                ReturnStrategy::AutomaticValidatedReturnToProduct),
            SensorSelectionPhase::ProductFailureDetected,
            SensorPeltierPermission::Blocked, RunSensorMode::Product);
        state.criticalSafetyEventPending = true;
        const auto request = sensorSelectionRequest(
            state, 91U, SensorSelectionUserAction::RecheckProduct, true);
        const auto decision =
            decideApplySensorSelectionAction(state, request, plausibility);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }
    // RecheckProduct, dessen Automat ohnehin NoChange liefert, bleibt
    // erlaubt (nichts zu verwerfen).
    {
        auto state = withSensorPhase(
            startedProgramStateWithReturnStrategy(
                ReturnStrategy::AutomaticValidatedReturnToProduct),
            SensorSelectionPhase::AirFallbackActive,
            SensorPeltierPermission::Allowed, RunSensorMode::Air);
        state.criticalSafetyEventPending = true;
        const auto request = sensorSelectionRequest(
            state, 92U, SensorSelectionUserAction::RecheckProduct, true);
        const auto decision = decideApplySensorSelectionAction(
            state, request,
            plausibilityWith(validSnapshot(), failedSnapshot(),
                             validSnapshot()));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NoChange),
                              static_cast<int>(decision.status));
    }

    // Externes safetyAllowsChange=false blockiert unabhaengig von
    // criticalSafetyEventPending - es ersetzt die interne Invariante nicht,
    // wird aber selbst ebenfalls verlangt.
    {
        auto state = withSensorPhase(startedProgramStateWithReturnStrategy(
                                         ReturnStrategy::ManualReturnToProduct),
                                     SensorSelectionPhase::UserDecisionRequired,
                                     SensorPeltierPermission::Blocked,
                                     RunSensorMode::Product);
        TEST_ASSERT_FALSE(state.criticalSafetyEventPending);
        const auto request = sensorSelectionRequest(
            state, 93U, SensorSelectionUserAction::ContinueWithAir, false);
        const auto decision =
            decideApplySensorSelectionAction(state, request, plausibility);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }
}

// #21, 6.14.3/Review-Fund aus Commit 3: ManualRunPlan::values.sensorMode
// dupliziert den Modus und muss bei jedem Moduswechsel mitgefuehrt werden,
// auch auf dem manuellen Kommandopfad (nicht nur ueber persistSensorSelection).
void test_sensor_selection_action_mirrors_mode_into_manual_run_plan() {
    auto blocked = withSensorPhase(
        startedManualProductState(), SensorSelectionPhase::UserDecisionRequired,
        SensorPeltierPermission::Blocked, RunSensorMode::Product);
    TEST_ASSERT_TRUE(blocked.activeManualRun.has_value());
    const auto request = sensorSelectionRequest(
        blocked, 105U, SensorSelectionUserAction::ContinueWithAir);
    const auto decision = decideApplySensorSelectionAction(
        blocked, request,
        plausibilityWith(validSnapshot(), failedSnapshot(), validSnapshot()));
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.after.activeManualRun.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Air),
        static_cast<int>(decision.after.activeManualRun->values.sensorMode));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Air),
        static_cast<int>(*decision.after.activeRunSensorMode));
}

void test_sensor_selection_action_already_processed_repeats_no_second_effect() {
    auto blocked = withSensorPhase(startedProgramStateWithReturnStrategy(
                                       ReturnStrategy::ManualReturnToProduct),
                                   SensorSelectionPhase::UserDecisionRequired,
                                   SensorPeltierPermission::Blocked,
                                   RunSensorMode::Product);
    const auto plausibility =
        plausibilityWith(validSnapshot(), failedSnapshot(), validSnapshot());
    const auto request = sensorSelectionRequest(
        blocked, 100U, SensorSelectionUserAction::ContinueWithAir);
    const auto decision =
        decideApplySensorSelectionAction(blocked, request, plausibility);
    TEST_ASSERT_TRUE(decision.proposed());
    auto current = blocked;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::Applied),
                          static_cast<int>(applyRunCommand(current, decision)));

    const auto repeat =
        decideApplySensorSelectionAction(current, request, plausibility);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::AlreadyProcessed),
                          static_cast<int>(repeat.status));
    TEST_ASSERT_EQUAL_UINT32(0U, repeat.effectCount);
}

// #21, 6.14.2 (Review-Blocking 1): applyRunCommand ist generisch fuer jeden
// CommandKind - eine zwischen Entscheidung und Anwendung veraenderte
// Sensorselektion (z. B. ein inzwischen eingetretener SafeLocked-Zustand)
// darf niemals stillschweigend von einer noch auf dem alten Zustand
// basierenden Entscheidung ueberschrieben werden.
void test_apply_run_command_staleness_regression_for_sensor_selection_and_other_commands() {
    // Fall 1: das neue Kommando selbst.
    {
        auto state = withSensorPhase(startedProgramStateWithReturnStrategy(
                                         ReturnStrategy::ManualReturnToProduct),
                                     SensorSelectionPhase::UserDecisionRequired,
                                     SensorPeltierPermission::Blocked,
                                     RunSensorMode::Product);
        const auto request = sensorSelectionRequest(
            state, 110U, SensorSelectionUserAction::ContinueWithAir);
        const auto decision = decideApplySensorSelectionAction(
            state, request,
            plausibilityWith(validSnapshot(), failedSnapshot(),
                             validSnapshot()));
        TEST_ASSERT_TRUE(decision.proposed());

        // Zwischenzeitliche automatische Bewertung: SafeLocked.
        state.sensorSelectionRuntime.phase = SensorSelectionPhase::SafeLocked;
        state.sensorSelectionRuntime.permission =
            SensorPeltierPermission::Blocked;

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(CommandStatus::StaleState),
            static_cast<int>(applyRunCommand(state, decision)));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::SafeLocked),
            static_cast<int>(state.sensorSelectionRuntime.phase));
    }
    // Fall 2: ein bestehender, unveraenderter Kommandotyp (AdjustRun) -
    // beweist, dass die erweiterte Vergleichsliste jeden Pfad schuetzt.
    {
        auto state = withSensorPhase(startedProgramStateWithReturnStrategy(
                                         ReturnStrategy::ManualReturnToProduct),
                                     SensorSelectionPhase::NormalProduct,
                                     SensorPeltierPermission::Allowed,
                                     RunSensorMode::Product);
        auto request = targetChange(state, 111U, 39.0, 200U);
        const auto decision = decideRunAdjustment(state, request);
        TEST_ASSERT_TRUE(decision.proposed());

        state.sensorSelectionRuntime.phase = SensorSelectionPhase::SafeLocked;
        state.sensorSelectionRuntime.permission =
            SensorPeltierPermission::Blocked;

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(CommandStatus::StaleState),
            static_cast<int>(applyRunCommand(state, decision)));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::SafeLocked),
            static_cast<int>(state.sensorSelectionRuntime.phase));
    }
}

// #21, 6.14.6: die neuen Felder werden bei jedem terminalen Pfad
// zurueckgesetzt; ein direkt anschliessender Start beginnt nicht mit einer
// uebernommenen Sensorselektion des vorherigen Laufs.
// Schema 3 (#18): populates the six clearActiveRunState()-managed
// recovery-/progress fields on `state` with non-default values, so a
// terminal-path regression test can prove they are actually reset instead of
// merely happening to already be at their zero value.
void populateRecoveryAndProgressFieldsForClearRegression(
    RunCommandState& state) {
    PendingRecoveryAnchor anchor;
    anchor.originalProcessState.state = ProcessState::Fermenting;
    anchor.knownPhaseSecondsAtOriginalCheckpoint = 5U;
    state.pendingRecoveryAnchor = anchor;
    state.recoveryBootAnchorMonotonicMillis = 10U;
    RecoveryEpisodeEvidence episode;
    episode.weightedProgressSegmentId = 3U;
    state.lastRecoveryEpisodeEvidence = episode;
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{20U, std::nullopt}};
    state.nominalRecoveryAdjustment =
        NominalRecoveryAdjustmentState{7U, 1U, 7U};
    state.runProgress.basis = RunProgressBasis::PartialUnknownHistory;
    state.runProgress.observedRunSeconds = 42U;
    state.runProgress.weightedProgress = WeightedProgressState{};
    // Absichtlich NICHT von clearActiveRunState() zurueckgesetzt (5.20, 5.14):
    // bleibt unveraendert, um das nachzuweisen.
    state.recoveryTemperatureEvidence.lastKnown.product.filteredCelsius = 6.5;
    state.recoveryEpisodeRevision = 9U;
}

void assertRecoveryAndProgressFieldsWereClearedButNotEvidenceOrRevision(
    const RunCommandState& after) {
    TEST_ASSERT_FALSE(after.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_FALSE(after.recoveryBootAnchorMonotonicMillis.has_value());
    TEST_ASSERT_FALSE(after.lastRecoveryEpisodeEvidence.has_value());
    TEST_ASSERT_FALSE(after.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_FALSE(after.nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunProgressBasis::KnownTotal),
                          static_cast<int>(after.runProgress.basis));
    TEST_ASSERT_EQUAL_UINT32(0U, after.runProgress.observedRunSeconds);
    TEST_ASSERT_FALSE(after.runProgress.weightedProgress.has_value());
    // recoveryTemperatureEvidence (laufend fortgeschrieben, kein
    // laufgebundenes Diagnosefeld) und recoveryEpisodeRevision (monotoner
    // Zaehler wie runRevision) bleiben bewusst unberuehrt.
    TEST_ASSERT_TRUE(after.recoveryTemperatureEvidence.lastKnown.product
                         .filteredCelsius.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(
        6.5,
        *after.recoveryTemperatureEvidence.lastKnown.product.filteredCelsius);
    TEST_ASSERT_EQUAL_UINT32(9U, after.recoveryEpisodeRevision);
}

void test_clear_active_run_state_regressions_across_terminal_paths() {
    // AbortAndTurnOff.
    {
        auto state = withSensorPhase(startedProgramStateWithReturnStrategy(
                                         ReturnStrategy::ManualReturnToProduct),
                                     SensorSelectionPhase::AirFallbackActive,
                                     SensorPeltierPermission::Allowed,
                                     RunSensorMode::Air);
        populateRecoveryAndProgressFieldsForClearRegression(state);
        StopRequest request{envelope(200U, state), StopOption::AbortAndTurnOff,
                            std::nullopt, false};
        const auto decision = decideStop(state, request);
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::NoActiveRun),
            static_cast<int>(decision.after.sensorSelectionRuntime.phase));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorPeltierPermission::Blocked),
            static_cast<int>(decision.after.sensorSelectionRuntime.permission));
        TEST_ASSERT_FALSE(decision.after.sensorSelection.has_value());
        assertRecoveryAndProgressFieldsWereClearedButNotEvidenceOrRevision(
            decision.after);

        // Ein neuer Start unmittelbar danach beginnt mit einer vollstaendig
        // neuen Sensorselektion (#21, 6.14.6/9.4), nicht mit der
        // uebernommenen des vorherigen (AirFallbackActive/FallbackActive)
        // Laufs.
        auto afterAbort = state;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(CommandStatus::Applied),
            static_cast<int>(applyRunCommand(afterAbort, decision)));
        const auto restart =
            decideProgramStart(afterAbort, programStart(afterAbort, 201U));
        TEST_ASSERT_TRUE(restart.proposed());
        TEST_ASSERT_TRUE(restart.after.sensorSelection.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionProvenance::InitialSelection),
            static_cast<int>(restart.after.sensorSelection->provenance));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::NormalProduct),
            static_cast<int>(restart.after.sensorSelectionRuntime.phase));
    }
    // AcknowledgeCompletion (regulaerer Abschluss).
    {
        auto state = withSensorPhase(startedProgramStateWithReturnStrategy(
                                         ReturnStrategy::ManualReturnToProduct),
                                     SensorSelectionPhase::NormalProduct,
                                     SensorPeltierPermission::Allowed,
                                     RunSensorMode::Product);
        state.processState.state = ProcessState::Completed;
        state.processState.targetReachStartedAtMillis = 0U;
        state.processState.targetReachWarningIssued = false;
        populateRecoveryAndProgressFieldsForClearRegression(state);
        CompletionRequest request{envelope(202U, state), false, std::nullopt,
                                  false};
        const auto decision = decideCompletion(state, request);
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::NoActiveRun),
            static_cast<int>(decision.after.sensorSelectionRuntime.phase));
        TEST_ASSERT_FALSE(decision.after.sensorSelection.has_value());
        assertRecoveryAndProgressFieldsWereClearedButNotEvidenceOrRevision(
            decision.after);
    }
}

// ---------------------------------------------------------------------------
// #21, 6.5/6.8/6.11: Startvertragsanschluss (Commit 5)
// ---------------------------------------------------------------------------

ProgramStartRequest programStartWithPreference(
    const RunCommandState& state, CommandId id, SensorPreference preference,
    RunSensorMode requestedMode, bool productSensorValid,
    bool airSensorValid = true, bool coolingSensorValid = true) {
    auto request = programStart(state, id);
    request.program.program.sensorPreference = preference;
    // 6.13 Regel 1/2/3/4: ProductRequired/AirOnly verlangen eine dazu
    // passende Policy/Ruecklaufstrategie, sonst waere schon das
    // ProgramDocument (nicht die Startmatrix) ungueltig.
    if (preference == SensorPreference::ProductRequired) {
        request.program.program.productSensorFailure.policy =
            ProductSensorFailurePolicy::WaitForUser;
        request.program.program.productSensorFailure.fallbackDelaySeconds =
            std::nullopt;
    } else if (preference == SensorPreference::AirOnly) {
        request.program.program.productSensorFailure.returnStrategy =
            ReturnStrategy::RemainOnAirUntilEnd;
        request.program.program.productSensorFailure.fallbackDelaySeconds =
            std::nullopt;
    }
    request.sensorMode = requestedMode;
    request.productSensorValid = productSensorValid;
    request.airSensorValid = airSensorValid;
    request.coolingSensorValid = coolingSensorValid;
    return request;
}

// #21, 6.5: alle elf Zeilen der Startmatrix.
void test_program_start_sensor_matrix_covers_all_eleven_rows() {
    // Zeile 1: ProductIfAvailableElseAir + Product + Valid -> Product.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state, programStartWithPreference(
                       state, 1U, SensorPreference::ProductIfAvailableElseAir,
                       RunSensorMode::Product, true));
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunSensorMode::Product),
            static_cast<int>(*decision.after.activeRunSensorMode));
        TEST_ASSERT_FALSE(decision.startSensorSelectionNotice.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::NormalProduct),
            static_cast<int>(decision.after.sensorSelectionRuntime.phase));
    }
    // Zeile 2: ProductIfAvailableElseAir + Product + nicht Valid ->
    // automatischer Ersatz auf Air, StartSensorSelectionNotice gesetzt.
    {
        auto state = standbyState();
        auto request = programStartWithPreference(
            state, 2U, SensorPreference::ProductIfAvailableElseAir,
            RunSensorMode::Product, false);
        const auto preview = decideProgramStart(state, request);
        // Vorschau vor Bestaetigung: bereits sichtbar (6.11), wie startSummary.
        request.envelope.confirmed = true;
        const auto decision = decideProgramStart(state, request);
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunSensorMode::Air),
            static_cast<int>(*decision.after.activeRunSensorMode));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::AirFallbackActive),
            static_cast<int>(decision.after.sensorSelectionRuntime.phase));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionProvenance::FallbackActive),
            static_cast<int>(decision.after.sensorSelection->provenance));
        TEST_ASSERT_TRUE(decision.startSensorSelectionNotice.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunSensorMode::Product),
            static_cast<int>(
                decision.startSensorSelectionNotice->requestedMode));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunSensorMode::Air),
            static_cast<int>(
                decision.startSensorSelectionNotice->effectiveMode));
        TEST_ASSERT_EQUAL_UINT32(
            decision.after.runRevision,
            decision.startSensorSelectionNotice->runRevision);
        // Bereits vor Bestaetigung sichtbar (6.11), mit demselben Wert.
        TEST_ASSERT_TRUE(preview.startSensorSelectionNotice.has_value());
        TEST_ASSERT_EQUAL_UINT32(
            decision.startSensorSelectionNotice->runRevision,
            preview.startSensorSelectionNotice->runRevision);
    }
    // Zeile 3: ProductIfAvailableElseAir + Air -> Air, keine Notice.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state, programStartWithPreference(
                       state, 3U, SensorPreference::ProductIfAvailableElseAir,
                       RunSensorMode::Air, false));
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunSensorMode::Air),
            static_cast<int>(*decision.after.activeRunSensorMode));
        TEST_ASSERT_FALSE(decision.startSensorSelectionNotice.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::NormalAir),
            static_cast<int>(decision.after.sensorSelectionRuntime.phase));
    }
    // Zeile 4: AirProductOptional + Product + Valid -> Product.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state, programStartWithPreference(
                       state, 4U, SensorPreference::AirProductOptional,
                       RunSensorMode::Product, true));
        TEST_ASSERT_TRUE(decision.proposed());
    }
    // Zeile 5: AirProductOptional + Product + nicht Valid -> abgelehnt.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state, programStartWithPreference(
                       state, 5U, SensorPreference::AirProductOptional,
                       RunSensorMode::Product, false));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }
    // Zeile 6: AirProductOptional + Air -> Air.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state, programStartWithPreference(
                       state, 6U, SensorPreference::AirProductOptional,
                       RunSensorMode::Air, false));
        TEST_ASSERT_TRUE(decision.proposed());
    }
    // Zeile 7: ProductRequired + Product + Valid -> Product.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state, programStartWithPreference(state, 7U,
                                              SensorPreference::ProductRequired,
                                              RunSensorMode::Product, true));
        TEST_ASSERT_TRUE(decision.proposed());
    }
    // Zeile 8: ProductRequired + Product + nicht Valid -> abgelehnt.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state, programStartWithPreference(state, 8U,
                                              SensorPreference::ProductRequired,
                                              RunSensorMode::Product, false));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }
    // Zeile 9: ProductRequired + Air -> abgelehnt.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state, programStartWithPreference(state, 9U,
                                              SensorPreference::ProductRequired,
                                              RunSensorMode::Air, false));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }
    // Zeile 10: AirOnly + Product -> abgelehnt.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state,
            programStartWithPreference(state, 10U, SensorPreference::AirOnly,
                                       RunSensorMode::Product, true));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }
    // Zeile 11: AirOnly + Air -> Air.
    {
        auto state = standbyState();
        const auto decision = decideProgramStart(
            state,
            programStartWithPreference(state, 11U, SensorPreference::AirOnly,
                                       RunSensorMode::Air, false));
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::NormalAir),
            static_cast<int>(decision.after.sensorSelectionRuntime.phase));
    }
}

// #21, 6.5: Vorbedingung gilt unabhaengig von SensorPreference/Modus - kein
// Sonderfall pro Zeile.
void test_program_start_rejects_uniformly_without_air_or_cooling() {
    auto state = standbyState();
    auto request =
        programStartWithPreference(state, 1U, SensorPreference::AirOnly,
                                   RunSensorMode::Air, false, false, true);
    auto decision = decideProgramStart(state, request);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                          static_cast<int>(decision.status));
    assertRejectedWithoutStateMutation(decision);

    request =
        programStartWithPreference(state, 2U, SensorPreference::AirOnly,
                                   RunSensorMode::Air, false, true, false);
    decision = decideProgramStart(state, request);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                          static_cast<int>(decision.status));
    assertRejectedWithoutStateMutation(decision);
}

// #21, 6.8: produktgefuehrter manueller Lauf - fester, nicht konfigurierbarer
// Vertrag (kein automatischer Ersatz, WaitForUser-artiges Verhalten).
void test_manual_start_product_mode_fixed_contract() {
    // Produkt gueltig: NormalProduct/Allowed.
    {
        auto state = standbyState();
        auto plan = manualPlan("manual-product-ok");
        plan.sensorMode = RunSensorMode::Product;
        const auto decision =
            decideManualStart(state, manualStart(state, 1U, plan));
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::NormalProduct),
            static_cast<int>(decision.after.sensorSelectionRuntime.phase));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorPeltierPermission::Allowed),
            static_cast<int>(decision.after.sensorSelectionRuntime.permission));
        TEST_ASSERT_FALSE(decision.startSensorSelectionNotice.has_value());
    }
    // Produkt zum Startzeitpunkt ungueltig: kein Ersatz, direkt
    // UserDecisionRequired/Blocked (6.8, fest wie WaitForUser).
    {
        auto state = standbyState();
        auto plan = manualPlan("manual-product-fail");
        plan.sensorMode = RunSensorMode::Product;
        auto request = manualStart(state, 2U, plan, true, true, true, false);
        const auto decision = decideManualStart(state, request);
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunSensorMode::Product),
            static_cast<int>(*decision.after.activeRunSensorMode));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorSelectionPhase::UserDecisionRequired),
            static_cast<int>(decision.after.sensorSelectionRuntime.phase));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SensorPeltierPermission::Blocked),
            static_cast<int>(decision.after.sensorSelectionRuntime.permission));
        TEST_ASSERT_FALSE(decision.startSensorSelectionNotice.has_value());
    }
    // Vorbedingung gilt auch fuer manuelle Starts.
    {
        auto state = standbyState();
        auto request =
            manualStart(state, 3U, manualPlan("manual-air"), true, false, true);
        const auto decision = decideManualStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
    }
}

// #21, 6.5/6.14.6: der Kuehl-Ersatzlauf nach Abbruch/Abschluss ist ebenfalls
// ein aktiver Lauf und bekommt dieselbe Erstbefuellung wie jeder Start.
void test_cooling_replacement_run_gets_fresh_sensor_selection() {
    auto state = startedProgramState();
    StopRequest request{envelope(2U, state), StopOption::AbortAndCool,
                        manualPlan("cool-after-abort"), true};
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    const auto decision = decideStop(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.after.activeManualRun.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::NormalAir),
        static_cast<int>(decision.after.sensorSelectionRuntime.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Allowed),
        static_cast<int>(decision.after.sensorSelectionRuntime.permission));
    TEST_ASSERT_TRUE(decision.after.sensorSelection.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionProvenance::InitialSelection),
        static_cast<int>(decision.after.sensorSelection->provenance));
}

// Korrekturauftrag Befund 2, Pflichttest "Kuehl-Ersatzlauf ohne gueltige
// feste Sensoren": der Ersatzlauf darf NormalAir/Allowed nicht per Konvention
// annehmen. Ohne explizite Air-/Cooling-Evidenz bleibt die Erstbefuellung
// fail-closed Blocked - auch ueber decideCompletion (derselbe gemeinsame
// Pfad).
void test_cooling_replacement_run_without_valid_fixed_sensors_stays_blocked() {
    auto state = startedProgramState();
    StopRequest request{envelope(2U, state), StopOption::AbortAndCool,
                        manualPlan("cool-after-abort"), true};
    request.airSensorValid = false;
    request.coolingSensorValid = false;
    const auto decision = decideStop(state, request);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.after.activeManualRun.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::NormalAir),
        static_cast<int>(decision.after.sensorSelectionRuntime.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Blocked),
        static_cast<int>(decision.after.sensorSelectionRuntime.permission));

    auto completedState = startedProgramState();
    completedState.processState.state = ProcessState::Completed;
    completedState.processState.targetReachStartedAtMillis = 0U;
    completedState.processState.targetReachWarningIssued = false;
    CompletionRequest completion{envelope(3U, completedState), true,
                                 manualPlan("cool-after-completion"), true};
    completion.airSensorValid = false;
    completion.coolingSensorValid = true;  // einzeln ungueltig genuegt.
    const auto completionDecision =
        decideCompletion(completedState, completion);
    TEST_ASSERT_TRUE(completionDecision.proposed());
    TEST_ASSERT_TRUE(completionDecision.after.activeManualRun.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Blocked),
        static_cast<int>(
            completionDecision.after.sensorSelectionRuntime.permission));
}

void test_start_decision_into_reuses_destination_without_stale_fields() {
    const auto state = standbyState();
    auto programRequest = programStart(state, 1U);
    CommandDecision destination;
    decideProgramStartInto(state, programRequest, destination);
    TEST_ASSERT_TRUE(destination.proposed());
    TEST_ASSERT_TRUE(destination.startSummary.has_value());
    TEST_ASSERT_TRUE(destination.effectCount > 0U);

    programRequest = programStart(state, 2U);
    programRequest.safetyAllowsStart = false;
    decideProgramStartInto(state, programRequest, destination);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                          static_cast<int>(destination.status));
    TEST_ASSERT_FALSE(destination.startSummary.has_value());
    TEST_ASSERT_FALSE(destination.adjustmentPreview.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, destination.effectCount);
    TEST_ASSERT_FALSE(destination.sensorSelectionApplyStatus.has_value());
    TEST_ASSERT_FALSE(destination.sensorSelectionEvent.has_value());
    TEST_ASSERT_FALSE(destination.sensorSelectionNotice.has_value());
    TEST_ASSERT_FALSE(destination.startSensorSelectionNotice.has_value());
    TEST_ASSERT_FALSE(
        destination.committedControlContextTransition.has_value());
    TEST_ASSERT_TRUE(equalRunCommandStates(destination.before, state));
    TEST_ASSERT_TRUE(equalRunCommandStates(destination.after, state));

    auto manualRequest = manualStart(state, 3U, manualPlan("hold"));
    decideManualStartInto(state, manualRequest, destination);
    TEST_ASSERT_TRUE(destination.proposed());
    TEST_ASSERT_TRUE(destination.effectCount > 0U);
    manualRequest = manualStart(state, 4U, manualPlan("hold"), false);
    decideManualStartInto(state, manualRequest, destination);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::SafetyRejected),
                          static_cast<int>(destination.status));
    TEST_ASSERT_FALSE(destination.startSummary.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, destination.effectCount);
    TEST_ASSERT_TRUE(equalRunCommandStates(destination.before, state));
    TEST_ASSERT_TRUE(equalRunCommandStates(destination.after, state));
}

void test_legacy_start_deciders_delegate_to_inplace_core() {
    const auto state = standbyState();
    const auto programRequest = programStart(state, 11U);
    CommandDecision programInto;
    decideProgramStartInto(state, programRequest, programInto);
    const auto programLegacy = decideProgramStart(state, programRequest);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(programLegacy.status),
                          static_cast<int>(programInto.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(programLegacy.kind),
                          static_cast<int>(programInto.kind));
    TEST_ASSERT_EQUAL_UINT32(programLegacy.effectCount,
                             programInto.effectCount);
    TEST_ASSERT_TRUE(
        equalRunCommandStates(programLegacy.before, programInto.before));
    TEST_ASSERT_TRUE(
        equalRunCommandStates(programLegacy.after, programInto.after));
    TEST_ASSERT_EQUAL(programLegacy.startSummary.has_value(),
                      programInto.startSummary.has_value());

    const auto manualRequest = manualStart(state, 12U, manualPlan("hold"));
    CommandDecision manualInto;
    decideManualStartInto(state, manualRequest, manualInto);
    const auto manualLegacy = decideManualStart(state, manualRequest);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(manualLegacy.status),
                          static_cast<int>(manualInto.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(manualLegacy.kind),
                          static_cast<int>(manualInto.kind));
    TEST_ASSERT_EQUAL_UINT32(manualLegacy.effectCount, manualInto.effectCount);
    TEST_ASSERT_TRUE(
        equalRunCommandStates(manualLegacy.before, manualInto.before));
    TEST_ASSERT_TRUE(
        equalRunCommandStates(manualLegacy.after, manualInto.after));
    TEST_ASSERT_EQUAL(manualLegacy.startSummary.has_value(),
                      manualInto.startSummary.has_value());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(
        test_program_start_requires_confirmation_safety_and_current_context);
    RUN_TEST(test_run_id_boundary_is_shared_by_program_and_manual_start);
    RUN_TEST(
        test_start_summary_is_available_before_confirmation_but_never_masks_rejections);
    RUN_TEST(
        test_manual_start_summary_is_available_before_confirmation_but_never_masks_rejections);
    RUN_TEST(test_program_start_is_two_stage_and_contains_summary);
    RUN_TEST(test_manual_plans_have_no_duration_and_use_canonical_start_states);
    RUN_TEST(
        test_display_and_web_conflict_is_first_applied_without_source_priority);
    RUN_TEST(test_stop_back_is_inert_and_abort_off_is_atomic);
    RUN_TEST(test_stop_rejects_unknown_option_without_mutation);
    RUN_TEST(test_abort_and_cool_validates_replacement_before_commit);
    RUN_TEST(test_completion_can_return_to_standby_or_start_manual_cooling);
    RUN_TEST(test_target_adjustments_requalify_before_fermentation_only);
    RUN_TEST(test_duration_adjustment_restarts_remaining_timer_and_allows_zero);
    RUN_TEST(
        test_duration_adjustment_folds_observed_time_and_resets_recovery_baseline);
    RUN_TEST(test_target_only_adjustment_keeps_recovery_and_timer_baseline);
    RUN_TEST(
        test_recovery_time_correction_is_cumulative_bounded_and_idempotent);
    RUN_TEST(test_nominal_recovery_correction_does_not_change_r1_phase_timer);
    RUN_TEST(
        test_late_run_adjustment_rejections_discard_the_complete_candidate);
    RUN_TEST(test_composed_cooling_rejections_discard_the_complete_candidate);
    RUN_TEST(test_adjustments_are_rejected_in_inappropriate_states);
    RUN_TEST(test_message_priority_acknowledgement_and_mute_are_independent);
    RUN_TEST(test_fault_reset_requires_current_qualified_evaluation);
    RUN_TEST(test_critical_safety_blocks_run_commands_but_not_message_commands);
    RUN_TEST(test_critical_safety_event_invalidates_a_pending_comfort_decision);
    RUN_TEST(test_domain_revision_conflicts_are_rejected_without_mutation);
    RUN_TEST(test_processed_command_ids_form_a_bounded_rolling_window);
    RUN_TEST(
        test_run_revision_overflow_is_rejected_for_every_run_mutating_command);
    RUN_TEST(test_run_revision_capacity_does_not_mask_prior_domain_results);
    RUN_TEST(test_message_and_fault_revision_overflow_is_rejected);
    RUN_TEST(
        test_sensor_selection_action_continue_with_air_valid_and_invalid_states);
    RUN_TEST(
        test_sensor_selection_action_return_to_product_valid_and_invalid_states);
    RUN_TEST(
        test_sensor_selection_action_recheck_product_ram_only_transport_and_idempotency);
    RUN_TEST(
        test_sensor_selection_action_no_change_leaves_everything_untouched);
    RUN_TEST(test_sensor_selection_action_safety_pending_matrix);
    RUN_TEST(test_sensor_selection_action_mirrors_mode_into_manual_run_plan);
    RUN_TEST(
        test_sensor_selection_action_already_processed_repeats_no_second_effect);
    RUN_TEST(
        test_apply_run_command_staleness_regression_for_sensor_selection_and_other_commands);
    RUN_TEST(test_clear_active_run_state_regressions_across_terminal_paths);
    RUN_TEST(test_program_start_sensor_matrix_covers_all_eleven_rows);
    RUN_TEST(test_program_start_rejects_uniformly_without_air_or_cooling);
    RUN_TEST(test_manual_start_product_mode_fixed_contract);
    RUN_TEST(test_cooling_replacement_run_gets_fresh_sensor_selection);
    RUN_TEST(
        test_cooling_replacement_run_without_valid_fixed_sensors_stays_blocked);
    RUN_TEST(test_start_decision_into_reuses_destination_without_stale_fields);
    RUN_TEST(test_legacy_start_deciders_delegate_to_inplace_core);
    return UNITY_END();
}
