#include <unity.h>

#include <array>
#include <cstdint>
#include <limits>

#include "run_commands.hpp"
#include "standard_program_catalog.hpp"

namespace {

using namespace fermentation;

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
        ManualStartRequest request{envelope(1U, state), manualPlan("hold"),
                                   true};
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
        ManualStartRequest request{envelope(1U, state), plan, true};
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
        ManualStartRequest request{envelope(1U, state), manualPlan("hold"),
                                   false};
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
    ManualStartRequest withoutPreheat{envelope(1U, state), manualPlan("hold"),
                                      true};
    const auto direct = decideManualStart(state, withoutPreheat);
    TEST_ASSERT_TRUE(direct.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(direct.after.processState.state));
    TEST_ASSERT_FALSE(direct.startSummary->durationMinutes.has_value());

    ManualStartRequest withPreheat{envelope(2U, state),
                                   manualPlan("preheat-hold", true), true};
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
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::NoChange),
                          static_cast<int>(decideStop(state, back).status));

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
    }

    // Abschlussquittierung gelingt ebenfalls noch; der zweite Uebergang darf
    // den abgeschlossenen Ursprungslauf bei Ablehnung nicht teilweise loeschen.
    {
        auto completed = startedProgramState();
        completed.processState.state = ProcessState::Completed;
        completed.processState.transitionSequence = almostFull;
        CompletionRequest request{envelope(2U, completed), true,
                                  manualPlan("cool"), true};
        const auto decision = decideCompletion(completed, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::InvalidInput),
                              static_cast<int>(decision.status));
        assertRejectedWithoutStateMutation(decision);
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
        ManualStartRequest request{envelope(1U, state), manualPlan("hold"),
                                   true};
        const auto decision = decideManualStart(state, request);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(CommandStatus::CapacityReached),
                              static_cast<int>(decision.status));
        TEST_ASSERT_TRUE(decision.startSummary.has_value());
        TEST_ASSERT_FALSE(decision.after.activeManualRun.has_value());
        assertRejectedWithoutStateMutation(decision);

        state.runRevision = max - 1U;
        ManualStartRequest okRequest{envelope(1U, state), manualPlan("hold"),
                                     true};
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
        ManualStartRequest request{envelope(1U, state), manualPlan("hold"),
                                   true};
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

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(
        test_program_start_requires_confirmation_safety_and_current_context);
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
    return UNITY_END();
}
