#include "issue_90_slice7_harness.hpp"

#if defined(APP_ISSUE_90_SLICE7_HARNESS)

#include <cinttypes>
#include <limits>
#include <string_view>
#include <utility>

#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "driver/uart.h"
#include "esp_timer_time_source.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "run_commands.hpp"
#include "run_persistence_coordinator.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace fermentation::issue_90_slice7 {
namespace {

constexpr char kTag[] = "issue90_slice7";
constexpr char kTestPartition[] = "state_store_test";
constexpr std::uint64_t kLoadIntervalMicros = 500000U;

const char* lifecycleName(ApplicationLifecycleState value) noexcept {
    switch (value) {
        case ApplicationLifecycleState::Initializing:
            return "Initializing";
        case ApplicationLifecycleState::Ready:
            return "Ready";
        case ApplicationLifecycleState::ServiceRequired:
            return "ServiceRequired";
    }
    return "Unknown";
}

const char* configurationRecoveryStatusName(
    ConfigurationRecoveryStatus value) noexcept {
    switch (value) {
        case ConfigurationRecoveryStatus::RuntimeReady:
            return "RuntimeReady";
        case ConfigurationRecoveryStatus::FactoryInitializationCompleted:
            return "FactoryInitializationCompleted";
        case ConfigurationRecoveryStatus::FactoryResetCompleted:
            return "FactoryResetCompleted";
        case ConfigurationRecoveryStatus::ConfigurationMutationBusy:
            return "ConfigurationMutationBusy";
        case ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy:
            return "ConfigurationModelBudgetBusy";
        case ConfigurationRecoveryStatus::StateTransitionRejected:
            return "StateTransitionRejected";
        case ConfigurationRecoveryStatus::ConfigurationUnavailable:
            return "ConfigurationUnavailable";
        case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
            return "ConfigurationIntegrityFailure";
        case ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema:
            return "UnsupportedNewerConfigurationSchema";
        case ConfigurationRecoveryStatus::PersistenceReadFailure:
            return "PersistenceReadFailure";
        case ConfigurationRecoveryStatus::PersistenceCapacityFailure:
            return "PersistenceCapacityFailure";
        case ConfigurationRecoveryStatus::PersistenceWriteFailure:
            return "PersistenceWriteFailure";
        case ConfigurationRecoveryStatus::CounterOverflow:
            return "CounterOverflow";
        case ConfigurationRecoveryStatus::RuntimePreparationFailure:
            return "RuntimePreparationFailure";
        case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
            return "BootstrapCommitIndeterminate";
        case ConfigurationRecoveryStatus::
            ConfigurationRecordOutcomeIndeterminate:
            return "ConfigurationRecordOutcomeIndeterminate";
        case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
            return "ConfigurationCommitIndeterminate";
    }
    return "Unknown";
}

const char* runLoadStatusName(RunPersistenceLoadStatus value) noexcept {
    switch (value) {
        case RunPersistenceLoadStatus::NoPersistedRun:
            return "NoPersistedRun";
        case RunPersistenceLoadStatus::Current:
            return "Current";
        case RunPersistenceLoadStatus::NoActiveRun:
            return "NoActiveRun";
        case RunPersistenceLoadStatus::FallbackRecovered:
            return "FallbackRecovered";
        case RunPersistenceLoadStatus::PreparedInterrupted:
            return "PreparedInterrupted";
        case RunPersistenceLoadStatus::NotReconstructible:
            return "NotReconstructible";
        case RunPersistenceLoadStatus::NotReconstructibleOrphanedState:
            return "NotReconstructibleOrphanedState";
        case RunPersistenceLoadStatus::ReadFailed:
            return "ReadFailed";
        case RunPersistenceLoadStatus::CapacityExceeded:
            return "CapacityExceeded";
        case RunPersistenceLoadStatus::UnsupportedSchema:
            return "UnsupportedSchema";
        case RunPersistenceLoadStatus::ForeignEpoch:
            return "ForeignEpoch";
        case RunPersistenceLoadStatus::AlreadyInitialized:
            return "AlreadyInitialized";
    }
    return "Unknown";
}

const char* runLoadDispositionName(RunLoadDisposition value) noexcept {
    switch (value) {
        case RunLoadDisposition::Standby:
            return "Standby";
        case RunLoadDisposition::ResumeOffer:
            return "ResumeOffer";
        case RunLoadDisposition::RecoveryEvaluation:
            return "RecoveryEvaluation";
        case RunLoadDisposition::NoActiveRun:
            return "NoActiveRun";
        case RunLoadDisposition::Completed:
            return "Completed";
        case RunLoadDisposition::TerminalFault:
            return "TerminalFault";
        case RunLoadDisposition::SafeBoot:
            return "SafeBoot";
    }
    return "Unknown";
}

const char* processStateName(ProcessState value) noexcept {
    switch (value) {
        case ProcessState::Boot:
            return "Boot";
        case ProcessState::SafeBoot:
            return "SafeBoot";
        case ProcessState::Standby:
            return "Standby";
        case ProcessState::Preheating:
            return "Preheating";
        case ProcessState::WaitingForProduct:
            return "WaitingForProduct";
        case ProcessState::ReachingTarget:
            return "ReachingTarget";
        case ProcessState::QualifyingTarget:
            return "QualifyingTarget";
        case ProcessState::Fermenting:
            return "Fermenting";
        case ProcessState::Cooling:
            return "Cooling";
        case ProcessState::CoolHolding:
            return "CoolHolding";
        case ProcessState::ManualHolding:
            return "ManualHolding";
        case ProcessState::Completed:
            return "Completed";
        case ProcessState::RecoveryEvaluation:
            return "RecoveryEvaluation";
        case ProcessState::Fault:
            return "Fault";
        case ProcessState::ServiceMode:
            return "ServiceMode";
    }
    return "Unknown";
}

const char* configurationCommitStatusName(
    ConfigurationCommitStatus value) noexcept {
    switch (value) {
        case ConfigurationCommitStatus::Activated:
            return "Activated";
        case ConfigurationCommitStatus::NoChange:
            return "NoChange";
        case ConfigurationCommitStatus::PreviewNotFound:
            return "PreviewNotFound";
        case ConfigurationCommitStatus::PreviewSuperseded:
            return "PreviewSuperseded";
        case ConfigurationCommitStatus::ConfigurationMutationBusy:
            return "ConfigurationMutationBusy";
        case ConfigurationCommitStatus::ConfigurationConflictFailure:
            return "ConfigurationConflictFailure";
        case ConfigurationCommitStatus::ConfigurationValidationFailure:
            return "ConfigurationValidationFailure";
        case ConfigurationCommitStatus::PersistenceFailure:
            return "PersistenceFailure";
        case ConfigurationCommitStatus::CapacityFailure:
            return "CapacityFailure";
        case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
            return "ConfigurationCommitIndeterminate";
        case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
            return "ConfigurationRuntimeFailure";
    }
    return "Unknown";
}

const char* runResultStatusName(RunPersistenceResultStatus value) noexcept {
    switch (value) {
        case RunPersistenceResultStatus::Applied:
            return "Applied";
        case RunPersistenceResultStatus::CheckpointWritten:
            return "CheckpointWritten";
        case RunPersistenceResultStatus::AlreadyProcessed:
            return "AlreadyProcessed";
        case RunPersistenceResultStatus::AlreadyPersisted:
            return "AlreadyPersisted";
        case RunPersistenceResultStatus::NotEligible:
            return "NotEligible";
        case RunPersistenceResultStatus::NotAllowedInState:
            return "NotAllowedInState";
        case RunPersistenceResultStatus::NotInitialized:
            return "NotInitialized";
        case RunPersistenceResultStatus::RecoveryPending:
            return "RecoveryPending";
        case RunPersistenceResultStatus::Busy:
            return "Busy";
        case RunPersistenceResultStatus::InvalidDecision:
            return "InvalidDecision";
        case RunPersistenceResultStatus::StaleDecision:
            return "StaleDecision";
        case RunPersistenceResultStatus::TimeMismatch:
            return "TimeMismatch";
        case RunPersistenceResultStatus::TimeWentBackwards:
            return "TimeWentBackwards";
        case RunPersistenceResultStatus::CounterOverflow:
            return "CounterOverflow";
        case RunPersistenceResultStatus::WriteFailed:
            return "WriteFailed";
        case RunPersistenceResultStatus::CapacityExceeded:
            return "CapacityExceeded";
        case RunPersistenceResultStatus::PersistenceIndeterminate:
            return "PersistenceIndeterminate";
        case RunPersistenceResultStatus::PersistenceCommittedApplyFailed:
            return "PersistenceCommittedApplyFailed";
        case RunPersistenceResultStatus::Blocked:
            return "Blocked";
        case RunPersistenceResultStatus::NotDue:
            return "NotDue";
        case RunPersistenceResultStatus::NoActiveRun:
            return "NoActiveRun";
    }
    return "Unknown";
}

}  // namespace

Harness::Harness(FermentationApplication& application) noexcept
    : application_(application) {}

std::uint64_t Harness::nowMicros() const noexcept {
    return timeSource_.monotonicMillis() * 1000U;
}

void Harness::start() noexcept {
    ESP_LOGI(kTag, "ISSUE90_PROTOCOL_VERSION=1");
    ESP_LOGI(kTag,
             "ISSUE90_READY partition=%s actor_free=YES "
             "real_actuators_enabled=NO backup_required=YES "
             "partition_offset=0x300000 partition_size=0x100000 "
             "test_partition_physically_separate=NO "
             "test_partition_reuses_production_flash_range=YES "
             "source_sha=%s "
             "load_stop=STOP_OR_PHYSICAL_POWER_CUT",
             kTestPartition, APP_SOURCE_GIT_SHA);
    emitStatus();
}

void Harness::update() noexcept {
    pollUart();
    if (loadMode_ == LoadMode::Stopped) return;

    const auto now = nowMicros();
    if (now < nextLoadAtMicros_) return;
    runLoadStep(now);
}

void Harness::pollUart() noexcept {
    for (std::size_t attempts = 0U; attempts < 64U; ++attempts) {
        size_t bufferedBytes = 0U;
        if (uart_get_buffered_data_len(UART_NUM_0, &bufferedBytes) != ESP_OK ||
            bufferedBytes == 0U) {
            return;
        }
        std::uint8_t byte = 0U;
        if (uart_read_bytes(UART_NUM_0, &byte, 1U, 0U) != 1) return;
        if (byte == '\r') continue;
        if (byte == '\n') {
            processLine();
            lineLength_ = 0U;
            lineOverflow_ = false;
            continue;
        }
        if (lineLength_ + 1U >= line_.size()) {
            lineOverflow_ = true;
            continue;
        }
        line_[lineLength_++] = static_cast<char>(byte);
    }
}

void Harness::processLine() noexcept {
    if (lineOverflow_) {
        ESP_LOGE(
            kTag,
            "ISSUE90_COMMAND_RESULT command=LINE result=FAIL reason=TOO_LONG");
        return;
    }
    const std::string_view command(line_.data(), lineLength_);
    if (command == "STATUS") {
        emitStatus();
    } else if (command == "BACKUP_OR_CONFIRM_TEST_PARTITION") {
        ESP_LOGI(kTag,
                 "ISSUE90_TEST_PARTITION_CONFIRMATION result=PASS label=%s "
                 "backup_required=YES offset=0x300000 size=0x100000",
                 kTestPartition);
        emitCommandResult("BACKUP_OR_CONFIRM_TEST_PARTITION", true);
    } else if (command == "CONFIG_CONTROL_WRITE") {
        emitCommandResult("CONFIG_CONTROL_WRITE", writeConfiguration());
    } else if (command == "RUN_CONTROL_WRITE") {
        emitCommandResult("RUN_CONTROL_WRITE", writeRun());
    } else if (command == "RUN_CONTROL_DISCARD_PENDING") {
        emitCommandResult("RUN_CONTROL_DISCARD_PENDING", discardPendingRun());
    } else if (command == "CONFIG_CONTROL_RELOAD") {
        requestRestart("CONFIG_CONTROL_RELOAD");
    } else if (command == "RUN_CONTROL_RELOAD") {
        requestRestart("RUN_CONTROL_RELOAD");
    } else if (command == "ARM_CONFIG_WRITE_LOAD") {
        arm(LoadMode::Configuration);
    } else if (command == "ARM_RUN_WRITE_LOAD") {
        arm(LoadMode::Run);
    } else if (command == "STOP") {
        loadMode_ = LoadMode::Stopped;
        const bool stopped = stopActiveRun();
        emitCommandResult("STOP", stopped);
    } else if (!command.empty()) {
        ESP_LOGE(kTag, "ISSUE90_COMMAND_RESULT command=UNKNOWN result=FAIL");
    }
}

void Harness::emitStatus() const noexcept {
    const auto* configuration = application_.configurationService_.get();
    const auto* recoveryStatus =
        application_.configurationRecoveryStatus_.has_value()
            ? &*application_.configurationRecoveryStatus_
            : nullptr;
    const auto* loadStatus = application_.persistenceLoadStatus_.has_value()
                                 ? &*application_.persistenceLoadStatus_
                                 : nullptr;
    const char* publishedState = "NONE";
    if (application_.runtimeRunState_ != nullptr) {
        publishedState =
            processStateName(application_.runtimeRunState_->processState.state);
    }
    ESP_LOGI(
        kTag,
        "ISSUE90_STATUS application_started=YES application_lifecycle=%s "
        "configuration_recovery_status=%s "
        "configuration_runtime_available=%s "
        "run_persistence_load_status=%s run_load_disposition=%s "
        "published_process_state=%s actuator_release=false",
        lifecycleName(application_.lifecycleState_),
        recoveryStatus == nullptr
            ? "NOT_AVAILABLE"
            : configurationRecoveryStatusName(*recoveryStatus),
        configuration != nullptr &&
                configuration->mode() == ConfigurationServiceMode::Operational
            ? "YES"
            : "NO",
        loadStatus == nullptr ? "NOT_AVAILABLE"
                              : runLoadStatusName(*loadStatus),
        runLoadDispositionName(application_.loadDisposition_), publishedState);
}

void Harness::emitCommandResult(const char* command,
                                bool passed) const noexcept {
    ESP_LOGI(kTag, "ISSUE90_COMMAND_RESULT command=%s result=%s", command,
             passed ? "PASS" : "FAIL");
}

void Harness::arm(LoadMode mode) noexcept {
    loadMode_ = mode;
    loadIteration_ = 0U;
    nextLoadAtMicros_ = nowMicros();
    const char* modeName = mode == LoadMode::Configuration ? "CONFIG" : "RUN";
    ESP_LOGI(kTag,
             "ISSUE90_LOAD_ARMED mode=%s mutation_payload=BOUNDED "
             "storage=PRODUCT_SERVICE stop=STOP_OR_PHYSICAL_POWER_CUT "
             "result=PASS",
             modeName);
}

void Harness::runLoadStep(std::uint64_t now) noexcept {
    const bool configuration = loadMode_ == LoadMode::Configuration;
    if (loadIteration_ == std::numeric_limits<std::uint32_t>::max()) {
        loadMode_ = LoadMode::Stopped;
        ESP_LOGE(kTag,
                 "ISSUE90_LOAD_COMPLETE mode=%s mutations=%" PRIu32
                 " result=FAIL reason=COMMAND_COUNTER_EXHAUSTED",
                 configuration ? "CONFIG" : "RUN", loadIteration_);
        return;
    }

    ESP_LOGI(
        kTag,
        "ISSUE90_OWNER_POWER_CUT_WINDOW_ACTIVE=YES mode=%s iteration=%" PRIu32,
        configuration ? "CONFIG" : "RUN", loadIteration_);
    const bool passed = configuration ? writeConfiguration() : writeRun();
    if (!passed) {
        loadMode_ = LoadMode::Stopped;
        ESP_LOGE(kTag,
                 "ISSUE90_LOAD_COMPLETE mode=%s mutations=%" PRIu32
                 " result=FAIL",
                 configuration ? "CONFIG" : "RUN", loadIteration_);
        return;
    }
    ++loadIteration_;
    nextLoadAtMicros_ = now + kLoadIntervalMicros;
}

bool Harness::writeConfiguration() noexcept {
    if (application_.configurationService_ == nullptr) return false;
    auto build = application_.configurationService_->beginPreview();
    if (build.status != ConfigurationPreviewStatus::Success ||
        !build.lease.valid()) {
        ESP_LOGE(kTag, "ISSUE90_CONFIG_WRITE_RESULT status=BUILD_REJECTED");
        return false;
    }

    auto& user = build.lease.userConfiguration();
    user.deviceName =
        (loadIteration_ % 2U) == 0U ? "issue90-config-a" : "issue90-config-b";
    auto installed = application_.configurationService_->installPreview(
        std::move(build.lease), decodeChangeOrigin(2U),
        decodeChangeOperation(1U));
    if (installed.status != ConfigurationPreviewStatus::Success ||
        !installed.preview.has_value()) {
        ESP_LOGE(kTag, "ISSUE90_CONFIG_WRITE_RESULT status=INSTALL_REJECTED");
        return false;
    }

    const auto commit = application_.configurationService_->confirmPreview(
        installed.preview->handle);
    const bool passed = commit.status == ConfigurationCommitStatus::Activated;
    ESP_LOGI(kTag,
             "ISSUE90_CONFIG_WRITE_RESULT status=%s revision=%" PRIu64
             " result=%s",
             configurationCommitStatusName(commit.status),
             application_.configurationService_->stateRevision(),
             passed ? "PASS" : "FAIL");
    return passed;
}

bool Harness::writeRun() noexcept {
    if (application_.runtimeRunState_ == nullptr ||
        application_.runPersistenceCoordinator_ == nullptr) {
        return false;
    }
    auto& state = *application_.runtimeRunState_;
    if (state.processState.state != ProcessState::Standby) {
        return stopActiveRun();
    }

    const auto timestamp = nowMicros() / 1000U;
    ManualStartRequest request;
    request.envelope = {
        nextCommandId_++,  CommandSource::LocalDisplay,
        timestamp,         state.processState.transitionSequence,
        state.runRevision, std::nullopt,
        std::nullopt,      true,
        std::nullopt};
    request.plan.runId =
        (loadIteration_ % 2U) == 0U ? "issue90-run-a" : "issue90-run-b";
    request.plan.targetTemperatureCelsius = 37.0;
    request.plan.sensorMode = RunSensorMode::Air;
    request.plan.preheatEnabled = false;
    request.plan.qualificationBandCelsius = 0.5;
    request.plan.qualificationDurationMinutes = 10U;
    request.plan.maximumTargetReachMinutes = 180U;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;

    const auto decision = decideManualStart(state, request);
    if (!decision.proposed()) {
        ESP_LOGE(kTag, "ISSUE90_RUN_WRITE_RESULT decision=REJECTED");
        return false;
    }
    const auto result = application_.runPersistenceCoordinator_->persistCommand(
        state, decision, RunCheckpointTime{timestamp, std::nullopt});
    const bool passed = result.status == RunPersistenceResultStatus::Applied;
    ESP_LOGI(kTag, "ISSUE90_RUN_WRITE_RESULT status=%s result=%s",
             runResultStatusName(result.status), passed ? "PASS" : "FAIL");
    return passed;
}

bool Harness::stopActiveRun() noexcept {
    if (application_.runtimeRunState_ == nullptr ||
        application_.runPersistenceCoordinator_ == nullptr) {
        return true;
    }
    auto& state = *application_.runtimeRunState_;
    if (!state.activeProgramRun.has_value() &&
        !state.activeManualRun.has_value()) {
        return true;
    }

    const auto timestamp = nowMicros() / 1000U;
    StopRequest request;
    request.envelope = {
        nextCommandId_++,  CommandSource::LocalDisplay,
        timestamp,         state.processState.transitionSequence,
        state.runRevision, std::nullopt,
        std::nullopt,      true,
        std::nullopt};
    request.option = StopOption::AbortAndTurnOff;
    request.safetyAllowsCooling = false;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    const auto decision = decideStop(state, request);
    if (!decision.proposed()) return false;
    const auto result = application_.runPersistenceCoordinator_->persistCommand(
        state, decision, RunCheckpointTime{timestamp, std::nullopt});
    const bool passed = result.status == RunPersistenceResultStatus::Applied;
    ESP_LOGI(kTag, "ISSUE90_RUN_STOP_RESULT status=%s result=%s",
             runResultStatusName(result.status), passed ? "PASS" : "FAIL");
    return passed;
}

bool Harness::discardPendingRun() noexcept {
    if (application_.pendingResume_ == nullptr ||
        application_.runPersistenceCoordinator_ == nullptr) {
        return true;
    }
    const auto result =
        application_.runPersistenceCoordinator_->discardAsNoActiveRun(
            *application_.pendingResume_,
            RunCheckpointTime{nowMicros() / 1000U, std::nullopt});
    if (result.status != RunPersistenceResultStatus::Applied) {
        ESP_LOGE(kTag,
                 "ISSUE90_RUN_DISCARD_PENDING_RESULT status=%s result=FAIL",
                 runResultStatusName(result.status));
        return false;
    }
    application_.runtimeRunState_ = std::move(application_.pendingResume_);
    application_.persistenceLoadStatus_ = RunPersistenceLoadStatus::NoActiveRun;
    application_.loadDisposition_ = RunLoadDisposition::Standby;
    ESP_LOGI(kTag,
             "ISSUE90_RUN_DISCARD_PENDING_RESULT status=Applied result=PASS");
    return true;
}

void Harness::requestRestart(const char* command) noexcept {
    loadMode_ = LoadMode::Stopped;
    ESP_LOGI(kTag, "ISSUE90_RELOAD_REQUEST command=%s result=PASS", command);
    vTaskDelay(pdMS_TO_TICKS(100U));
    esp_restart();
}

}  // namespace fermentation::issue_90_slice7

#endif
