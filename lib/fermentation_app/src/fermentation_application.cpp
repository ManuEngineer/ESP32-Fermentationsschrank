#include "fermentation_application.hpp"

#include <new>
#include <utility>

#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "fermentation_ui_commands.hpp"
#include "run_persistence_coordinator.hpp"

namespace fermentation {
namespace {

FaultCode configurationFault(ConfigurationRecoveryStatus status) {
    switch (status) {
        case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
        case ConfigurationRecoveryStatus::UnsupportedNewerConfigurationSchema:
            return FaultCode::ConfigurationIntegrityFailure;
        case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
        case ConfigurationRecoveryStatus::
            ConfigurationRecordOutcomeIndeterminate:
        case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
            return FaultCode::ConfigurationCommitIndeterminate;
        default:
            return FaultCode::ConfigurationUnavailable;
    }
}

}  // namespace

FermentationApplication::~FermentationApplication() = default;

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    const device_platform::IResetCauseSource* resetCauseSource) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    timeSource_ = nullptr;
    stateStore_ = nullptr;
    storageEpoch_.reset();
    runIdentity_.reset();
    configurationRecoveryService_.reset();
    runPersistenceCoordinator_.reset();
    recoveryDisposition_.reset();
    lifecycleState_ = ApplicationLifecycleState::Ready;
    presentationState_ = PresentationState{};
    presentationState_.resetCause = resetCauseSource == nullptr
                                        ? device_platform::ResetCause::Unknown
                                        : resetCauseSource->resetCause();
    return true;
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::IResetCauseSource* resetCauseSource) {
    return beginPersistent(platformServices, store, timeZoneResolver, nullptr,
                           resetCauseSource);
}

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::ITimeSource& timeSource,
    const device_platform::IResetCauseSource* resetCauseSource) {
    return beginPersistent(platformServices, store, timeZoneResolver,
                           &timeSource, resetCauseSource);
}

bool FermentationApplication::beginPersistent(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::ITimeSource* timeSource,
    const device_platform::IResetCauseSource* resetCauseSource) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    timeSource_ = timeSource;
    stateStore_ = &store;
    storageEpoch_.reset();
    runIdentity_.reset();
    lifecycleState_ = ApplicationLifecycleState::Initializing;
    presentationState_ = PresentationState{};
    presentationState_.resetCause = resetCauseSource == nullptr
                                        ? device_platform::ResetCause::Unknown
                                        : resetCauseSource->resetCause();
    persistenceLoadStatus_.reset();
    loadDisposition_ = RunLoadDisposition::SafeBoot;
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    configurationRecoveryStatus_.reset();
#endif
    pendingResume_.reset();
    pendingFallbackResume_.reset();
    owningRecoveryEvidence_.reset();
    pendingRecoverySource_.reset();
    recoveryDisposition_.reset();
    runtimeRunState_.reset();
    runPersistenceCoordinator_.reset();
    configurationRecoveryService_.reset();
    configurationService_.reset();
    graphStore_.reset();
    mutationCoordinator_.reset();
    bootstrapStore_.reset();

    bootstrapStore_ = std::unique_ptr<ConfigurationBootstrapStore>{
        new (std::nothrow) ConfigurationBootstrapStore(store)};
    if (bootstrapStore_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    mutationCoordinator_ = std::unique_ptr<ConfigurationMutationCoordinator>{
        new (std::nothrow) ConfigurationMutationCoordinator()};
    if (mutationCoordinator_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    graphStore_ = std::unique_ptr<ConfigurationGraphStore>{
        new (std::nothrow) ConfigurationGraphStore(store, timeZoneResolver)};
    if (graphStore_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    configurationService_ = std::unique_ptr<ConfigurationService>{
        new (std::nothrow) ConfigurationService(
            *mutationCoordinator_, *graphStore_, timeZoneResolver)};
    if (configurationService_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    auto recovery = ConfigurationRecoveryService::create(
        store, *bootstrapStore_, *graphStore_, *configurationService_,
        *mutationCoordinator_);
    if (recovery == nullptr) {
        requireService(FaultCode::ConfigurationUnavailable);
        return true;
    }

    configurationRecoveryService_ = std::move(recovery);
    const auto configurationResult = configurationRecoveryService_->boot();
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    configurationRecoveryStatus_ = configurationResult.status;
#endif
    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        requireService(configurationFault(configurationResult.status));
        return true;
    }
    const auto epoch = runtime.lease.get().storageEpoch();
    storageEpoch_ = epoch;

    runPersistenceCoordinator_ = std::unique_ptr<RunPersistenceCoordinator>{
        new (std::nothrow)
            RunPersistenceCoordinator(store, epoch, RunCheckpointSchedule{})};
    if (runPersistenceCoordinator_ == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }

    auto loadResult = std::unique_ptr<RunPersistenceLoadResult>{
        new (std::nothrow) RunPersistenceLoadResult{}};
    if (loadResult == nullptr) {
        requireService(FaultCode::None, true);
        return true;
    }
    runPersistenceCoordinator_->loadAndInitializeInto(*loadResult);

    if (const auto highWater = runPersistenceCoordinator_->commandIdHighWater();
        highWater.has_value()) {
        auto identity = ApplicationRunIdentity::create(epoch, highWater);
        if (!identity.has_value()) {
            requireService(FaultCode::RunPersistenceUntrusted);
            return true;
        }
        runIdentity_ = std::unique_ptr<ApplicationRunIdentity>{
            new (std::nothrow) ApplicationRunIdentity(std::move(*identity))};
        if (runIdentity_ == nullptr) {
            requireService(FaultCode::None, true);
            return true;
        }
    }

    persistenceLoadStatus_ = loadResult->status;
    const RunPersistenceSnapshot* snapshot = nullptr;
    if (loadResult->snapshot.has_value()) {
        snapshot = loadResult->snapshot.operator->();
    }
    loadDisposition_ =
        boot_classification::classifyRunLoad(loadResult->status, snapshot);
    const auto classification =
        boot_classification::classify(loadResult->status, snapshot);

    const RunCheckpointTime bootTime = currentCheckpointTime();
    if (!processBootClassification(classification, snapshot, bootTime)) {
        return true;
    }

    lifecycleState_ = ApplicationLifecycleState::Ready;
    return true;
}

bool FermentationApplication::processBootClassification(
    BootClassification classification, const RunPersistenceSnapshot* snapshot,
    const RunCheckpointTime& bootTime) {
    switch (classification) {
        case BootClassification::NoRun:
            return publishStandby();
        case BootClassification::ResumeOffer:
            return prepareResumeOffer(snapshot);
        case BootClassification::RecoveryEvaluation:
            return evaluateCurrentRecovery(snapshot, bootTime);
        case BootClassification::FallbackSelectionRequired:
            return prepareFallbackSelection(snapshot);
        case BootClassification::DiscardableRun:
        case BootClassification::CompletedRun:
        case BootClassification::TerminalRunFault:
            return processTerminalClassification(classification, snapshot,
                                                 bootTime);
        case BootClassification::SafeBoot:
        case BootClassification::Unresolved:
            requireService(FaultCode::RunPersistenceUntrusted);
            return false;
    }
    requireService(FaultCode::RunPersistenceUntrusted);
    return false;
}

bool FermentationApplication::prepareResumeOffer(
    const RunPersistenceSnapshot* snapshot) {
    if (snapshot == nullptr) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    pendingResume_ =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (pendingResume_ == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    if (!restoreRunPersistenceSnapshotInto(*snapshot, *pendingResume_)) {
        pendingResume_.reset();
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    return true;
}

bool FermentationApplication::prepareFallbackSelection(
    const RunPersistenceSnapshot* snapshot) {
    if (snapshot == nullptr) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    pendingFallbackResume_ =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (pendingFallbackResume_ == nullptr ||
        !restoreRunPersistenceSnapshotInto(*snapshot,
                                           *pendingFallbackResume_)) {
        pendingFallbackResume_.reset();
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    // The unresolved fallback is retained for an explicit user choice.  It
    // is never published as an active runtime state and therefore cannot
    // satisfy the actuator interlock before a successful persistence apply.
    return true;
}

bool FermentationApplication::evaluateCurrentRecovery(
    const RunPersistenceSnapshot* snapshot, const RunCheckpointTime& bootTime) {
    if (snapshot == nullptr) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    pendingRecoverySource_ =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (pendingRecoverySource_ == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    if (!restoreRunPersistenceSnapshotInto(*snapshot,
                                           *pendingRecoverySource_)) {
        pendingRecoverySource_.reset();
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }

    const auto evaluation =
        runPersistenceCoordinator_->evaluateCurrentFermentingRecovery(
            *pendingRecoverySource_, bootTime);
    recoveryDisposition_ = evaluation.disposition;
    if (evaluation.disposition == RecoveryDisposition::WaitingForTrustedTime) {
        return enterRecoveryEvaluationRamState(*pendingRecoverySource_);
    }
    if (evaluation.disposition == RecoveryDisposition::CurrentRunRecoverable) {
        runtimeRunState_ = std::move(pendingRecoverySource_);
        return true;
    }
    return enterRecoveryEvaluationRamState(*pendingRecoverySource_);
}

bool FermentationApplication::processTerminalClassification(
    BootClassification classification, const RunPersistenceSnapshot* snapshot,
    const RunCheckpointTime& bootTime) {
    if (snapshot == nullptr) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    auto target =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (target == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    if (!restoreRunPersistenceSnapshotInto(*snapshot, *target)) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }

    const auto persisted =
        classification == BootClassification::DiscardableRun
            ? runPersistenceCoordinator_->discardAsNoActiveRun(*target,
                                                               bootTime)
            : runPersistenceCoordinator_->activateR1EligibleRun(
                  *target, bootTime, nullptr);
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    runtimeRunState_ = std::move(target);
    return true;
}

void FermentationApplication::update() { reevaluateWaitingForTrustedTime(); }

void FermentationApplication::publishOwningRecoveryEvidence(
    const CrossRolePlausibilityContext& evidence) {
    owningRecoveryEvidence_ = evidence;
}

RunPersistenceResult FermentationApplication::resumeFallback(
    const FermentationUiResumeFallbackCommand& command) {
    if (pendingFallbackResume_ == nullptr ||
        runPersistenceCoordinator_ == nullptr) {
        RunPersistenceResult unavailable;
        unavailable.status = RunPersistenceResultStatus::NotInitialized;
        return unavailable;
    }
    const auto& state = *pendingFallbackResume_;
    if (command.expected.expectedStateSequence !=
            state.processState.transitionSequence ||
        (command.expected.expectedRunRevision.has_value() &&
         *command.expected.expectedRunRevision != state.runRevision) ||
        (command.expected.expectedMessageRevision.has_value() &&
         *command.expected.expectedMessageRevision != state.messageRevision) ||
        (command.expected.expectedFaultRevision.has_value() &&
         *command.expected.expectedFaultRevision != state.faultRevision) ||
        (command.expected.expectedRecoveryEpisodeRevision.has_value() &&
         *command.expected.expectedRecoveryEpisodeRevision !=
             state.recoveryEpisodeRevision)) {
        RunPersistenceResult stale;
        stale.status = RunPersistenceResultStatus::StaleDecision;
        stale.coordinatorState =
            RunPersistenceCoordinatorState::FallbackRecoveryPending;
        return stale;
    }
    if (!command.confirmed) {
        RunPersistenceResult pending;
        pending.status = RunPersistenceResultStatus::RecoveryPending;
        pending.coordinatorState =
            RunPersistenceCoordinatorState::FallbackRecoveryPending;
        return pending;
    }
    if (!owningRecoveryEvidence_.has_value()) {
        RunPersistenceResult unavailable;
        unavailable.status = RunPersistenceResultStatus::RecoveryPending;
        unavailable.coordinatorState =
            RunPersistenceCoordinatorState::FallbackRecoveryPending;
        return unavailable;
    }
    // Evidence is a point-in-time owning observation.  Consume it before the
    // mutating coordinator attempt so neither a failed write nor a pending
    // trusted-time result can replay the same sensor/plausibility snapshot.
    const auto evidence = *owningRecoveryEvidence_;
    owningRecoveryEvidence_.reset();
    const auto outcome =
        runPersistenceCoordinator_->activateFallbackRecoveredRun(
            *pendingFallbackResume_, currentCheckpointTime(), evidence);
    if (outcome.persistenceResult.status ==
        RunPersistenceResultStatus::Applied) {
        // The coordinator's resultingState is the exact candidate that was
        // durably committed.  Adopt that value, rather than the pre-commit
        // retained fallback copy, so RAM/FSM cannot diverge from storage.
        runtimeRunState_ = std::unique_ptr<RunCommandState>{
            new (std::nothrow) RunCommandState(outcome.resultingState)};
        if (runtimeRunState_ == nullptr) {
            requireService(FaultCode::None, true);
            RunPersistenceResult failed;
            failed.status =
                RunPersistenceResultStatus::PersistenceCommittedApplyFailed;
            failed.step = RunPersistenceStep::RamApply;
            failed.technicalReason =
                RunPersistenceTechnicalReason::InvalidProjection;
            failed.durability = RunPersistenceDurability::MayHaveChanged;
            failed.coordinatorState =
                RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed;
            return failed;
        }
        pendingFallbackResume_.reset();
        if (outcome.resultingState.activeProgramRun.has_value() ||
            outcome.resultingState.activeManualRun.has_value()) {
            loadDisposition_ = RunLoadDisposition::ResumeOffer;
            persistenceLoadStatus_ = RunPersistenceLoadStatus::Current;
        } else {
            loadDisposition_ = RunLoadDisposition::NoActiveRun;
            persistenceLoadStatus_ = RunPersistenceLoadStatus::NoActiveRun;
            recoveryDisposition_.reset();
        }
    }
    return outcome.persistenceResult;
}

std::optional<device_platform::UiRequestId>
FermentationApplication::allocateUiRequestId() noexcept {
    if (runIdentity_ == nullptr) return std::nullopt;
    return runIdentity_->allocateUiRequestId();
}

std::optional<CommandId> FermentationApplication::allocateCommandId() noexcept {
    if (runIdentity_ == nullptr) return std::nullopt;
    return runIdentity_->allocateCommandId();
}

std::optional<std::string> FermentationApplication::makeRunId(
    CommandId startCommandId) const {
    if (runIdentity_ == nullptr) return std::nullopt;
    return runIdentity_->makeRunId(startCommandId);
}

ConfigurationRecoveryResult
FermentationApplication::beginAuthorizedFactoryReset() {
    ConfigurationRecoveryResult unavailable{
        ConfigurationRecoveryStatus::ConfigurationUnavailable, {}};
    if (configurationRecoveryService_ == nullptr ||
        configurationService_ == nullptr || stateStore_ == nullptr ||
        !storageEpoch_.has_value()) {
        return unavailable;
    }

    const auto previousEpoch = *storageEpoch_;
    const auto reset =
        configurationRecoveryService_->beginAuthorizedFactoryReset();
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    configurationRecoveryStatus_ = reset.status;
#endif
    if (reset.status != ConfigurationRecoveryStatus::FactoryResetCompleted) {
        return reset;
    }

    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        requireService(FaultCode::ConfigurationUnavailable);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    const auto currentEpoch = runtime.lease.get().storageEpoch();
    if (currentEpoch.value() == 0U || currentEpoch == previousEpoch) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }

    auto coordinator = std::unique_ptr<RunPersistenceCoordinator>{
        new (std::nothrow) RunPersistenceCoordinator(*stateStore_, currentEpoch,
                                                     RunCheckpointSchedule{})};
    if (coordinator == nullptr) {
        requireService(FaultCode::None, true);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    const AuthorizedRunEpochHandoffProof proof{previousEpoch, currentEpoch};
    const auto handoff = coordinator->completeAuthorizedEpochHandoff(proof);
    if (handoff.status != RunPersistenceResultStatus::Applied) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    runPersistenceCoordinator_ = std::move(coordinator);
    storageEpoch_ = currentEpoch;
    runIdentity_.reset();
    pendingResume_.reset();
    pendingFallbackResume_.reset();
    pendingRecoverySource_.reset();
    owningRecoveryEvidence_.reset();
    recoveryDisposition_.reset();
    runtimeRunState_.reset();

    auto loadResult = std::unique_ptr<RunPersistenceLoadResult>{
        new (std::nothrow) RunPersistenceLoadResult{}};
    if (loadResult == nullptr) {
        requireService(FaultCode::None, true);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    runPersistenceCoordinator_->loadAndInitializeInto(*loadResult);
    persistenceLoadStatus_ = loadResult->status;
    if (const auto highWater = runPersistenceCoordinator_->commandIdHighWater();
        highWater.has_value()) {
        auto identity = ApplicationRunIdentity::create(currentEpoch, highWater);
        if (!identity.has_value()) {
            requireService(FaultCode::RunPersistenceUntrusted);
            return {
                ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
        }
        runIdentity_ = std::unique_ptr<ApplicationRunIdentity>{
            new (std::nothrow) ApplicationRunIdentity(std::move(*identity))};
        if (runIdentity_ == nullptr) {
            requireService(FaultCode::None, true);
            return {
                ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
        }
    }
    const RunPersistenceSnapshot* snapshot =
        loadResult->snapshot.has_value() ? &*loadResult->snapshot : nullptr;
    loadDisposition_ =
        boot_classification::classifyRunLoad(loadResult->status, snapshot);
    const auto classification =
        boot_classification::classify(loadResult->status, snapshot);
    if (!processBootClassification(classification, snapshot,
                                   currentCheckpointTime())) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return {ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable,
                reset.diagnostics};
    }
    lifecycleState_ = ApplicationLifecycleState::Ready;
    return reset;
}

RunCheckpointTime FermentationApplication::currentCheckpointTime()
    const noexcept {
    if (timeSource_ == nullptr) {
        return RunCheckpointTime{};
    }
    return RunCheckpointTime{timeSource_->monotonicMillis(),
                             timeSource_->unixTimeSeconds()};
}

bool FermentationApplication::enterRecoveryEvaluationRamState(
    const RunCommandState& source) {
    auto target =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (target == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }
    *target = source;
    const auto now = currentCheckpointTime().monotonicMillis;
    if (runPersistenceCoordinator_ == nullptr ||
        !runPersistenceCoordinator_->prepareRecoveryEvaluationState(*target,
                                                                    now)) {
        requireService(FaultCode::RunPersistenceUntrusted);
        return false;
    }
    runtimeRunState_ = std::move(target);
    loadDisposition_ = RunLoadDisposition::RecoveryEvaluation;
    return true;
}

void FermentationApplication::reevaluateWaitingForTrustedTime() {
    if (!recoveryDisposition_.has_value() ||
        *recoveryDisposition_ != RecoveryDisposition::WaitingForTrustedTime ||
        pendingRecoverySource_ == nullptr ||
        runPersistenceCoordinator_ == nullptr || timeSource_ == nullptr) {
        return;
    }

    const auto evaluation =
        runPersistenceCoordinator_->evaluateCurrentFermentingRecovery(
            *pendingRecoverySource_, currentCheckpointTime());
    recoveryDisposition_ = evaluation.disposition;
    if (evaluation.disposition == RecoveryDisposition::WaitingForTrustedTime) {
        return;
    }
    if (evaluation.disposition == RecoveryDisposition::CurrentRunRecoverable) {
        runtimeRunState_ = std::move(pendingRecoverySource_);
        return;
    }
    static_cast<void>(enterRecoveryEvaluationRamState(*pendingRecoverySource_));
}

bool FermentationApplication::ready() const {
    return lifecycleState_ == ApplicationLifecycleState::Ready;
}

std::optional<ProcessRuntimeState>
FermentationApplication::publishedProcessState() const {
    if (runtimeRunState_ == nullptr) {
        return std::nullopt;
    }
    return runtimeRunState_->processState;
}

void FermentationApplication::requireService(
    FaultCode faultCode, bool applicationAllocationFailure) noexcept {
    lifecycleState_ = ApplicationLifecycleState::ServiceRequired;
    presentationState_.faultCode = faultCode;
    presentationState_.applicationAllocationFailure =
        applicationAllocationFailure;
}

bool FermentationApplication::publishStandby() {
    auto target =
        std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState{}};
    if (target == nullptr) {
        requireService(FaultCode::None, true);
        return false;
    }

    if (!establishBootCompletedStandby(target->processState, 0U)) {
        requireService(FaultCode::None);
        return false;
    }
    runtimeRunState_ = std::move(target);
    return true;
}

}  // namespace fermentation
