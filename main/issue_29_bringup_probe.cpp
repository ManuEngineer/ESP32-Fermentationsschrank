#include "issue_29_bringup_probe.hpp"

#if defined(APP_ISSUE_29_BRINGUP_PROBE)

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "configuration_limits.hpp"
#include "issue_29_bringup_fault_seam.hpp"
#include "program_limits.hpp"
#include "run_commands.hpp"
#include "run_limits.hpp"
#include "run_persistence_coordinator.hpp"
#include "safety_core.hpp"
#include "state_store.hpp"
#include "temperature_control_orchestrator.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace fermentation::issue_29_bringup {
namespace {

constexpr char kTag[] = "issue29_probe";
constexpr TickType_t kProbeWaitTicks = pdMS_TO_TICKS(10000U);
constexpr TickType_t kCleanupWaitTicks = pdMS_TO_TICKS(3000U);
constexpr TickType_t kProbeControlWaitTicks = pdMS_TO_TICKS(30000U);
constexpr UBaseType_t kProbePriority = tskIDLE_PRIORITY + 1U;
constexpr std::uint32_t kStartProbeNotification = 1U << 0U;
constexpr std::uint32_t kCleanupProbeNotification = 1U << 1U;
constexpr std::uint32_t kProbeReadyEvent = 1U << 2U;
constexpr std::uint32_t kProbeCompletedEvent = 1U << 3U;
constexpr std::uint32_t kProbeCleanupEvent = 1U << 4U;

// ESP-IDF 6.0.2 documents uxTaskGetStackHighWaterMark() as returning bytes
// on ESP32 (rather than the word unit used by stock FreeRTOS documentation).
// The four internal samples therefore use this API through its NULL/current-
// task form and expose the result as *_bytes.

// This is a diagnostic-task stack formula, not a product setting. The
// relevant non-inlined frames are summed along the actual deterministic
// candidate-allocation-failure path. The small script
// scripts/analyze_issue_29_stack.py verifies the corresponding .su/.ci
// artefacts and reproduces this result.
constexpr std::size_t kHeldObjectBytes =
    sizeof(CommandDecision) + sizeof(RunCommandState) +
    sizeof(ProgramStartRequest) + sizeof(RunPersistenceCoordinator) +
    sizeof(TemperatureControlApplicationOrchestrator) +
    sizeof(TemperatureController) + sizeof(ActuatorPlanner) +
    sizeof(TargetQualificationEvaluator) + sizeof(SafetyCore);
// esp32_bringup @ -Og, Xtensa GCC 15.2.0, -fstack-usage and
// -fcallgraph-info=su: probeTask + runProbe + persistFreshStartCommand +
// TemperatureControlApplicationOrchestrator::persistCommand +
// RunPersistenceCoordinator::persistCommand + result() = 62928 bytes.
// Individual .su values are not a cumulative bound; this value is.
constexpr std::size_t kMeasuredCallPathBytes = 62928U;
constexpr std::size_t kMeasuredCallPathSafetyBufferBytes = 4096U;
constexpr std::size_t kUnroundedProbeTaskStackBytes =
    kMeasuredCallPathBytes + kMeasuredCallPathSafetyBufferBytes;
// Keep the diagnostic allocation on a reproducible 1 KiB boundary after the
// measured cumulative path and bounded safety buffer have been accounted for.
constexpr std::size_t kProbeTaskStackBytes =
    ((kUnroundedProbeTaskStackBytes + 1023U) / 1024U) * 1024U;
static_assert(kProbeTaskStackBytes <=
              static_cast<std::size_t>(
                  std::numeric_limits<configSTACK_DEPTH_TYPE>::max()));
constexpr configSTACK_DEPTH_TYPE kProbeTaskStackDepth =
    static_cast<configSTACK_DEPTH_TYPE>(kProbeTaskStackBytes);

struct ResourceSample {
    std::uint32_t freeHeapBytes{0U};
    std::uint32_t minimumFreeHeapBytes{0U};
    std::size_t largestFreeBlockBytes{0U};
    UBaseType_t taskStackHighWaterMarkBytes{0U};
    UBaseType_t mainStackHighWaterMarkBytes{0U};
};

struct ProbeContext {
    TaskHandle_t caller{nullptr};
    TaskHandle_t task{nullptr};
    bool ready{false};
    bool completed{false};
    bool resourcePass{false};
    bool faultPass{false};
    bool taskFailed{false};
    bool decisionProposed{false};
    bool localApplyPass{false};
    bool localApplyMeasured{false};
    bool taskCleanupProven{false};
    bool afterTaskCleanupMeasured{false};
    bool cleanupHandoffReceived{false};
    bool stateUnchanged{false};
    bool persistenceUnchanged{false};
    bool actorReleaseObserved{false};
    bool safetyFailClosed{false};
    std::size_t storeWriteCount{0U};
    std::size_t storeReadCount{0U};
    RunPersistenceResultStatus faultStatus{RunPersistenceResultStatus::Blocked};
    RunPersistenceStep faultStep{RunPersistenceStep::None};
    RunPersistenceTechnicalReason faultTechnicalReason{
        RunPersistenceTechnicalReason::None};
    RunPersistenceDurability faultDurability{
        RunPersistenceDurability::Unchanged};
    ResourceSample beforeDecision;
    ResourceSample decisionHeld;
    ResourceSample localApply;
    ResourceSample afterDecisionRelease;
    ResourceSample beforeTaskCreate;
    ResourceSample afterTaskCreate;
    ResourceSample afterTaskCleanup;
    UBaseType_t readyTaskStackHighWaterMarkBytes{0U};
    UBaseType_t completionTaskStackHighWaterMarkBytes{0U};
};

ResourceSample sampleResources(TaskHandle_t task, TaskHandle_t mainTask,
                               bool sampleCurrentTask = false) {
    return {
        esp_get_free_heap_size(),
        esp_get_minimum_free_heap_size(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        sampleCurrentTask
            ? uxTaskGetStackHighWaterMark(nullptr)
            : (task == nullptr ? 0U : uxTaskGetStackHighWaterMark(task)),
        mainTask == nullptr ? 0U : uxTaskGetStackHighWaterMark(mainTask),
    };
}

void logSample(const char* label, const ResourceSample& sample) {
    ESP_LOGI(kTag,
             "%s free_heap_bytes=%" PRIu32 " minimum_free_heap_bytes=%" PRIu32
             " largest_free_block_bytes=%zu task_stack_hwm_bytes=%u"
             " main_stack_hwm_bytes=%u",
             label, sample.freeHeapBytes, sample.minimumFreeHeapBytes,
             sample.largestFreeBlockBytes,
             static_cast<unsigned>(sample.taskStackHighWaterMarkBytes),
             static_cast<unsigned>(sample.mainStackHighWaterMarkBytes));
}

std::string repeatedUtf8(std::size_t scalarCount) {
    std::string value;
    value.reserve(scalarCount * 2U);
    for (std::size_t index = 0U; index < scalarCount; ++index) {
        value.append("\xC3\xA4");
    }
    return value;
}

ProgramDocument maximalProgram() {
    ProgramDefinition program;
    program.id = std::string(run_limits::kMaximumRunIdBytes, 'p');
    program.name = repeatedUtf8(48U);
    program.notes = std::string(configuration_limits::kMaximumNotesBytes, 'n');
    program.builtIn = true;
    program.factoryCatalogEntry = true;
    program.resettable = true;
    program.userDeletable = true;
    program.installed = true;
    program.enabled = true;
    program.preheat = false;
    program.sensorPreference = SensorPreference::AirProductOptional;
    program.productSensorFailure.policy =
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
    program.productSensorFailure.fallbackDelaySeconds =
        program_limits::kMaximumFallbackDelaySeconds;
    program.productSensorFailure.returnStrategy =
        ReturnStrategy::AutomaticValidatedReturnToProduct;
    program.fermentationStages.push_back(
        {program_limits::kMaximumFermentationTemperatureCelsius,
         program_limits::kMaximumFermentationDurationMinutes});
    program.targetQualification.bandCelsius =
        program_limits::kMaximumQualificationBandCelsius;
    program.targetQualification.durationMinutes =
        program_limits::kMaximumQualificationDurationMinutes;
    program.maximumTargetReachMinutes =
        program_limits::kMaximumTargetReachMinutes;
    program.completion.mode = CompletionMode::FinishWithoutCooling;

    return {{kCurrentProgramSchemaVersion, kCurrentRequiredProgramFields},
            std::move(program)};
}

RunCommandState standbyState() {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    return state;
}

ProgramStartRequest maximalStartRequest() {
    ProgramStartRequest request;
    request.envelope = {1U,           CommandSource::LocalDisplay,
                        100U,         0U,
                        0U,           std::nullopt,
                        std::nullopt, true,
                        std::nullopt};
    request.runId = std::string(run_limits::kMaximumRunIdBytes, 'r');
    request.program = maximalProgram();
    request.sourceKind = ProgramSourceKind::FactoryCatalog;
    request.sourceProgramRevision = 1U;
    request.sensorMode = RunSensorMode::Air;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = false;
    return request;
}

class BringupStateStore final : public device_platform::IStateStore {
   public:
    [[nodiscard]] device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey&, const std::string&) override {
        ++writeCount_;
        // The fault probe must never create a durable record. A write would
        // itself be a failed probe observation, so return the existing
        // fail-closed, certainly-not-written status.
        return device_platform::StateStoreWriteStatus::WriteError;
    }

    [[nodiscard]] device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey&, std::size_t) const override {
        ++readCount_;
        return {device_platform::StateStoreReadStatus::NotFound, {}};
    }

    [[nodiscard]] std::size_t writeCount() const { return writeCount_; }
    [[nodiscard]] std::size_t readCount() const { return readCount_; }

   private:
    std::size_t writeCount_{0U};
    mutable std::size_t readCount_{0U};
};

class AllOffBidirectionalSink final
    : public device_platform::IBidirectionalActuatorSink {
   public:
    void setForward(bool enabled) override {
        ++callCount_;
        releaseObserved_ = releaseObserved_ || enabled;
    }

    void setReverse(bool enabled) override {
        ++callCount_;
        releaseObserved_ = releaseObserved_ || enabled;
    }

    [[nodiscard]] std::size_t callCount() const { return callCount_; }
    [[nodiscard]] bool releaseObserved() const { return releaseObserved_; }

   private:
    std::size_t callCount_{0U};
    bool releaseObserved_{false};
};

class AllOffBinarySink final : public device_platform::IBinaryOutputSink {
   public:
    void setEnabled(bool enabled) override {
        ++callCount_;
        releaseObserved_ = releaseObserved_ || enabled;
    }

    [[nodiscard]] std::size_t callCount() const { return callCount_; }
    [[nodiscard]] bool releaseObserved() const { return releaseObserved_; }

   private:
    std::size_t callCount_{0U};
    bool releaseObserved_{false};
};

bool unchangedStandbyState(const RunCommandState& state) {
    // The fault seam is before the existing candidate copy and this probe
    // starts from the canonical empty Standby state. Check every mutable
    // domain that can be non-default in that state, including all optional
    // run/recovery/persistence-facing fields and the command windows.
    const RunCommandState expected = standbyState();
    return equalProcessRuntimeState(state.processState,
                                    expected.processState) &&
           !state.activeProgramRun.has_value() &&
           !state.activeManualRun.has_value() &&
           !state.processRunSnapshot.has_value() && state.activeRunId.empty() &&
           !state.activeRunSensorMode.has_value() &&
           !state.sensorSelection.has_value() &&
           state.sensorSelectionRuntime == expected.sensorSelectionRuntime &&
           state.runRevision == 0U && state.messageCount == 0U &&
           state.messageRevision == 0U && state.faultRevision == 0U &&
           !state.criticalSafetyEventPending && state.commandSequence == 0U &&
           state.lastCommandMonotonicMillis == 0U &&
           state.processedCommandCount == 0U &&
           !state.pendingRecoveryAnchor.has_value() &&
           !state.recoveryBootAnchorMonotonicMillis.has_value() &&
           !state.lastRecoveryEpisodeEvidence.has_value() &&
           !state.priorBootPhaseElapsed.has_value() &&
           !state.nominalRecoveryAdjustment.has_value() &&
           state.recoveryEpisodeRevision == 0U;
}

void runProbe(ProbeContext& context) {
    RunCommandState current = standbyState();
    ProgramStartRequest request = maximalStartRequest();

    context.beforeDecision = sampleResources(nullptr, context.caller, true);
    std::optional<CommandDecision> decision;
    decision.emplace(decideProgramStart(current, request));
    context.decisionProposed = decision->proposed();
    context.decisionHeld = sampleResources(nullptr, context.caller, true);

    if (!context.decisionProposed) {
        context.taskFailed = true;
        return;
    }

    {
        RunCommandState candidate = current;
        const auto apply = issue_29_bringup::applyCandidateForResourceProbe(
            candidate, *decision);
        context.localApplyPass = apply == CommandStatus::Applied;
        if (context.localApplyPass) {
            // Keep the decision, the applied candidate, and their dynamic
            // contents alive while measuring the third planned point.
            context.localApply = sampleResources(nullptr, context.caller, true);
            context.localApplyMeasured = true;
        } else {
            context.taskFailed = true;
        }
    }

    // Release the full decision and candidate before starting the separate
    // error-contract probe. This keeps the resource and fault observations
    // distinct and makes the dynamic string/copy heap cost visible.
    decision.reset();
    context.afterDecisionRelease =
        sampleResources(nullptr, context.caller, true);

    BringupStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();

    TargetQualificationEvaluator evaluator;
    TemperatureController controller(TemperatureControlParameters{},
                                     IntegratorTransitionPolicy{});
    ActuatorPlanner planner(ActuatorPlannerParameters{});
    AllOffBidirectionalSink peltier;
    AllOffBinarySink outerFan;
    AllOffBinarySink innerFan;
    ActuatorPlanSinkDriver driver(peltier, outerFan, innerFan);
    SafetyCore safetyCore;
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator, planner, driver, safetyCore);

    issue_29_bringup::armCandidateAllocationFailure();
    RunCommandState faultState = standbyState();
    const auto faultDecision = decideProgramStart(faultState, request);
    const auto faultResult = application.persistFreshStartCommand(
        faultState, faultDecision, RunCheckpointTime{100U, std::nullopt});

    context.faultStatus = faultResult.status;
    context.faultStep = faultResult.step;
    context.faultTechnicalReason = faultResult.technicalReason;
    context.faultDurability = faultResult.durability;
    context.storeWriteCount = store.writeCount();
    context.storeReadCount = store.readCount();
    context.stateUnchanged = unchangedStandbyState(faultState);
    context.persistenceUnchanged =
        faultResult.durability == RunPersistenceDurability::Unchanged &&
        context.storeWriteCount == 0U &&
        loaded.status == RunPersistenceLoadStatus::NoPersistedRun;
    context.actorReleaseObserved =
        faultResult.effectCount != 0U || peltier.callCount() != 0U ||
        outerFan.callCount() != 0U || innerFan.callCount() != 0U ||
        peltier.releaseObserved() || outerFan.releaseObserved() ||
        innerFan.releaseObserved();
    context.safetyFailClosed = safetyCore.lastEvaluation().gate.status !=
                               ActuatorSafetyGateStatus::Allowed;
    context.faultPass =
        faultDecision.proposed() &&
        faultResult.status == RunPersistenceResultStatus::Blocked &&
        faultResult.step == RunPersistenceStep::CandidateApply &&
        context.stateUnchanged && context.persistenceUnchanged &&
        !context.actorReleaseObserved && context.safetyFailClosed;
    context.resourcePass =
        context.localApplyPass && context.localApplyMeasured &&
        context.beforeDecision.taskStackHighWaterMarkBytes > 0U &&
        context.decisionHeld.taskStackHighWaterMarkBytes > 0U &&
        context.localApply.taskStackHighWaterMarkBytes > 0U &&
        context.afterDecisionRelease.taskStackHighWaterMarkBytes > 0U;
}

bool waitForTaskControl(std::uint32_t& notification, TickType_t waitTicks) {
    notification = 0U;
    return xTaskNotifyWait(0U, std::numeric_limits<std::uint32_t>::max(),
                           &notification, waitTicks) == pdTRUE;
}

bool waitForCallerEvent(std::uint32_t expectedEvent, TickType_t waitTicks) {
    std::uint32_t notification = 0U;
    if (xTaskNotifyWait(0U, std::numeric_limits<std::uint32_t>::max(),
                        &notification, waitTicks) != pdTRUE) {
        return false;
    }
    return (notification & expectedEvent) != 0U;
}

[[noreturn]] void deleteProbeTask(ProbeContext& context,
                                  bool cleanupAlreadyRequested) {
    std::uint32_t notification = 0U;
    if (!cleanupAlreadyRequested &&
        (!waitForTaskControl(notification, kProbeControlWaitTicks) ||
         (notification & kCleanupProbeNotification) == 0U)) {
        context.taskFailed = true;
    }

    // Copy the only context value needed by the final handoff before sending
    // the event. After xTaskNotify returns, this worker deliberately performs
    // no further access through `context`; the caller may then safely leave
    // run() only after receiving the event.
    const TaskHandle_t caller = context.caller;
    if (caller != nullptr) {
        (void)xTaskNotify(caller, kProbeCleanupEvent, eSetBits);
    }
    vTaskDelete(nullptr);
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

void probeTask(void* argument) {
    auto* context = static_cast<ProbeContext*>(argument);
    context->readyTaskStackHighWaterMarkBytes =
        uxTaskGetStackHighWaterMark(nullptr);
    context->ready = true;
    (void)xTaskNotify(context->caller, kProbeReadyEvent, eSetBits);

    std::uint32_t notification = 0U;
    if (!waitForTaskControl(notification, kProbeControlWaitTicks) ||
        (notification & kStartProbeNotification) == 0U) {
        context->taskFailed = true;
        context->completed = true;
        (void)xTaskNotify(context->caller, kProbeCompletedEvent, eSetBits);
        deleteProbeTask(*context,
                        (notification & kCleanupProbeNotification) != 0U);
    }

    runProbe(*context);
    context->completed = true;
    context->completionTaskStackHighWaterMarkBytes =
        uxTaskGetStackHighWaterMark(nullptr);
    (void)xTaskNotify(context->caller, kProbeCompletedEvent, eSetBits);
    deleteProbeTask(*context, false);
}

void logProbeSummary(const ProbeContext& context) {
    ESP_LOGI(kTag,
             "stack_formula_bytes=%zu command_decision_bytes=%zu"
             " run_command_state_bytes=%zu persistence_coordinator_bytes=%zu"
             " held_object_bytes=%zu call_path_bytes=%zu"
             " call_path_safety_buffer_bytes=%zu"
             " configured_task_stack_bytes=%u",
             kProbeTaskStackBytes, sizeof(CommandDecision),
             sizeof(RunCommandState), sizeof(RunPersistenceCoordinator),
             kHeldObjectBytes, kMeasuredCallPathBytes,
             kMeasuredCallPathSafetyBufferBytes,
             static_cast<unsigned>(kProbeTaskStackDepth));
    logSample("before_decision", context.beforeDecision);
    logSample("decision_held", context.decisionHeld);
    logSample("local_apply_held", context.localApply);
    logSample("after_decision_release", context.afterDecisionRelease);
    logSample("before_task_create", context.beforeTaskCreate);
    logSample("after_task_create_blocked", context.afterTaskCreate);
    if (context.afterTaskCleanupMeasured) {
        logSample("after_task_cleanup", context.afterTaskCleanup);
    } else {
        ESP_LOGE(kTag, "after_task_cleanup status=BLOCKED");
    }
    ESP_LOGI(kTag, "task_ready_hwm_bytes=%u",
             static_cast<unsigned>(context.readyTaskStackHighWaterMarkBytes));
    ESP_LOGI(
        kTag, "task_completion_hwm_bytes=%u",
        static_cast<unsigned>(context.completionTaskStackHighWaterMarkBytes));
    ESP_LOGI(kTag,
             "fault_status=%u fault_step=%u fault_technical_reason=%u"
             " fault_durability=%u fault_pass=%s writes=%zu reads=%zu"
             " state_unchanged=%s"
             " persistence_unchanged=%s actor_release=%s"
             " safety_fail_closed=%s",
             static_cast<unsigned>(context.faultStatus),
             static_cast<unsigned>(context.faultStep),
             static_cast<unsigned>(context.faultTechnicalReason),
             static_cast<unsigned>(context.faultDurability),
             context.faultPass ? "PASS" : "FAILED", context.storeWriteCount,
             context.storeReadCount, context.stateUnchanged ? "true" : "false",
             context.persistenceUnchanged ? "true" : "false",
             context.actorReleaseObserved ? "true" : "false",
             context.safetyFailClosed ? "true" : "false");
    ESP_LOGI(kTag,
             "internal_resource_hwm_valid=%s local_apply_measured=%s"
             " cleanup_handoff_received=%s task_cleanup_proven=%s"
             " after_task_cleanup_measured=%s",
             context.resourcePass ? "true" : "false",
             context.localApplyMeasured ? "true" : "false",
             context.cleanupHandoffReceived ? "true" : "false",
             context.taskCleanupProven ? "true" : "false",
             context.afterTaskCleanupMeasured ? "true" : "false");
}

bool requestCleanupAndJoin(ProbeContext& context) {
    const TaskHandle_t task = context.task;
    if (task == nullptr ||
        xTaskNotify(task, kCleanupProbeNotification, eSetBits) != pdPASS) {
        ESP_LOGE(kTag,
                 "BLOCKED: cleanup request failed; retaining Context until"
                 " task termination is proven");
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    if (!waitForCallerEvent(kProbeCleanupEvent, kCleanupWaitTicks)) {
        context.taskFailed = true;
        ESP_LOGE(kTag,
                 "BLOCKED: cleanup handoff exceeded bounded wait;"
                 " retaining Context until worker handoff is received");
        // A timeout is not ownership proof. Keep this caller alive in a
        // fail-closed join until the worker has completed its final context
        // access and sent the cleanup event.
        while (!waitForCallerEvent(kProbeCleanupEvent, portMAX_DELAY)) {
        }
    }
    context.cleanupHandoffReceived = true;
    return true;
}

}  // namespace

bool run() {
    ProbeContext context;
    context.caller = xTaskGetCurrentTaskHandle();
    context.beforeTaskCreate = sampleResources(nullptr, context.caller);

    const auto createResult =
        xTaskCreate(probeTask, "issue29_probe", kProbeTaskStackDepth, &context,
                    kProbePriority, &context.task);
    if (createResult != pdPASS) {
        ESP_LOGE(kTag,
                 "FAILED: diagnostic task creation failed; no probe result"
                 " and no hardware PASS is possible");
        return false;
    }

    if (!waitForCallerEvent(kProbeReadyEvent, kProbeWaitTicks) ||
        !context.ready) {
        context.taskFailed = true;
        ESP_LOGE(kTag,
                 "BLOCKED: diagnostic task did not reach its blocked start"
                 " gate");
        (void)requestCleanupAndJoin(context);
        return false;
    }
    context.afterTaskCreate = sampleResources(context.task, context.caller);
    if (xTaskNotify(context.task, kStartProbeNotification, eSetBits) !=
        pdPASS) {
        context.taskFailed = true;
        ESP_LOGE(kTag, "BLOCKED: diagnostic task start notification failed");
        (void)requestCleanupAndJoin(context);
        return false;
    }

    if (!waitForCallerEvent(kProbeCompletedEvent, kProbeWaitTicks) ||
        !context.completed) {
        context.taskFailed = true;
        ESP_LOGE(kTag,
                 "BLOCKED: diagnostic task did not complete within the"
                 " bounded wait");
        (void)requestCleanupAndJoin(context);
        return false;
    }

    // Every post-create exit uses this same join. Receipt of the cleanup
    // event proves that the worker has performed its last access to context;
    // it is not confused with the later B2 heap-reclamation proof.
    (void)requestCleanupAndJoin(context);

    // The worker has completed its context handoff and is deleting itself.
    // Do not query its handle. The Idle task owns deferred TCB/stack
    // reclamation.
    //
    // The B2 proof is anchored on B1 (`afterTaskCreate`, sampled immediately
    // after this task's own creation and before the actual probe workload
    // starts), not on B0. Two independent real-hardware experiments on
    // esp32_bringup established measured cycle invariance: a 12 s wait did
    // not change the largest-free-block plateau, and a second, identically-
    // sized create/delete cycle caused no further net loss. These experiments
    // do not determine the concrete cause of the residual B0-to-B2 delta.
    // The exact B0 return is therefore not a suitable task-specific cleanup
    // gate. Comparing against B1 instead proves that at least this task's own
    // configured stack allocation (`kProbeTaskStackBytes`, already derived at
    // compile time from the measured call path, not an invented constant) was
    // returned to the heap after deletion. That is a lower bound attributable
    // to this task; it is not a general proof that every workload allocation
    // is leak-free.
    // largestFreeBlockBytes remains part of the recorded B2 sample below (it
    // must not be hidden), but is informational rather than a pass/fail gate.
    const TickType_t idleStart = xTaskGetTickCount();
    while ((xTaskGetTickCount() - idleStart) < kCleanupWaitTicks) {
        vTaskDelay(1U);
        const auto cleanupSample = sampleResources(nullptr, context.caller);
        if (cleanupSample.freeHeapBytes >=
                context.afterTaskCreate.freeHeapBytes &&
            (cleanupSample.freeHeapBytes -
             context.afterTaskCreate.freeHeapBytes) >=
                static_cast<std::uint32_t>(kProbeTaskStackBytes)) {
            context.afterTaskCleanup = cleanupSample;
            context.afterTaskCleanupMeasured = true;
            context.taskCleanupProven = true;
            break;
        }
    }
    if (!context.taskCleanupProven) {
        context.taskFailed = true;
        ESP_LOGE(kTag,
                 "BLOCKED: deferred diagnostic-task cleanup did not reclaim"
                 " at least its own configured stack allocation");
    }

    logProbeSummary(context);
    const bool stackObserved =
        context.readyTaskStackHighWaterMarkBytes > 0U &&
        context.completionTaskStackHighWaterMarkBytes > 0U &&
        context.afterTaskCreate.taskStackHighWaterMarkBytes > 0U;
    const bool completedSafely = context.completed && !context.taskFailed;
    const bool pass = completedSafely && stackObserved &&
                      context.taskCleanupProven &&
                      context.afterTaskCleanupMeasured &&
                      context.resourcePass && context.faultPass;
    ESP_LOGI(kTag, "result=%s", pass ? "PASS" : "FAILED");
    return pass;
}

}  // namespace fermentation::issue_29_bringup

#endif
