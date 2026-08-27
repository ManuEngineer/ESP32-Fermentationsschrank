#include "fermentation_application.hpp"

#include <new>
#include <utility>

#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
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
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
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
    runtimeRunState_.reset();
    runPersistenceCoordinator_.reset();
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

    const auto configurationResult = recovery->boot();
#if defined(APP_ISSUE_90_SLICE7_HARNESS)
    configurationRecoveryStatus_ = configurationResult.status;
#endif
    recovery.reset();
    const auto runtime = configurationService_->acquireRuntime();
    if (runtime.status != RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
        requireService(configurationFault(configurationResult.status));
        return true;
    }
    const auto epoch = runtime.lease.get().storageEpoch();

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

    persistenceLoadStatus_ = loadResult->status;
    const auto* snapshot =
        loadResult->snapshot.has_value() ? &*loadResult->snapshot : nullptr;
    loadDisposition_ =
        boot_classification::classifyRunLoad(loadResult->status, snapshot);
    const auto classification =
        boot_classification::classify(loadResult->status, snapshot);

    const RunCheckpointTime bootTime{};
    switch (classification) {
        case BootClassification::NoRun:
            if (!publishStandby()) {
                return true;
            }
            break;
        case BootClassification::ResumeOffer:
            if (snapshot == nullptr) {
                requireService(FaultCode::RunPersistenceUntrusted);
                return true;
            }
            pendingResume_ = std::unique_ptr<RunCommandState>{
                new (std::nothrow) RunCommandState{}};
            if (pendingResume_ == nullptr) {
                requireService(FaultCode::None, true);
                return true;
            }
            if (!restoreRunPersistenceSnapshotInto(*snapshot,
                                                   *pendingResume_)) {
                pendingResume_.reset();
                requireService(FaultCode::RunPersistenceUntrusted);
                return true;
            }
            break;
        case BootClassification::DiscardableRun:
        case BootClassification::CompletedRun:
        case BootClassification::TerminalRunFault: {
            if (snapshot == nullptr) {
                requireService(FaultCode::RunPersistenceUntrusted);
                return true;
            }
            auto target = std::unique_ptr<RunCommandState>{
                new (std::nothrow) RunCommandState{}};
            if (target == nullptr) {
                requireService(FaultCode::None, true);
                return true;
            }
            if (!restoreRunPersistenceSnapshotInto(*snapshot, *target)) {
                requireService(FaultCode::RunPersistenceUntrusted);
                return true;
            }

            const auto persisted =
                classification == BootClassification::DiscardableRun
                    ? runPersistenceCoordinator_->discardAsNoActiveRun(*target,
                                                                       bootTime)
                    : runPersistenceCoordinator_->activateR1EligibleRun(
                          *target, bootTime, nullptr);
            if (persisted.status != RunPersistenceResultStatus::Applied) {
                requireService(FaultCode::RunPersistenceUntrusted);
                return true;
            }
            runtimeRunState_ = std::move(target);
            break;
        }
        case BootClassification::SafeBoot:
        case BootClassification::Unresolved:
            requireService(FaultCode::RunPersistenceUntrusted);
            return true;
    }

    lifecycleState_ = ApplicationLifecycleState::Ready;
    return true;
}

void FermentationApplication::update() {
    // Die Fermentationslogik wird issueweise in diesem Modul implementiert.
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
