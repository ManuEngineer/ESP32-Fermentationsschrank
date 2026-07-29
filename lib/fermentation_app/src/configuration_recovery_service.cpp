#include "configuration_recovery_service.hpp"

#include <array>
#include <limits>
#include <utility>

#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "crc32.hpp"
#include "state_store_key.hpp"

namespace fermentation {
namespace {

device_platform::StateStoreKey key(const char* value) {
    auto result = device_platform::StateStoreKey::create(value);
    // All call sites pass compile-time keys from the validated storage
    // contract. NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return std::move(*result.key);
}

ConfigurationRecoveryStatus mapPrepare(
    InitialConfigurationPrepareStatus status) {
    switch (status) {
        case InitialConfigurationPrepareStatus::CapacityFailure:
            return ConfigurationRecoveryStatus::PersistenceCapacityFailure;
        case InitialConfigurationPrepareStatus::PersistenceFailure:
            return ConfigurationRecoveryStatus::PersistenceReadFailure;
        case InitialConfigurationPrepareStatus::UnsupportedNewerSchema:
            return ConfigurationRecoveryStatus::
                UnsupportedNewerConfigurationSchema;
        case InitialConfigurationPrepareStatus::InvalidCandidate:
        case InitialConfigurationPrepareStatus::IntegrityFailure:
        case InitialConfigurationPrepareStatus::NoSafeSlotAvailable:
            return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
        case InitialConfigurationPrepareStatus::Success:
            return ConfigurationRecoveryStatus::RuntimeReady;
    }
    return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
}

ConfigurationRecoveryStatus mapLoad(ConfigurationGraphLoadStatus status) {
    switch (status) {
        case ConfigurationGraphLoadStatus::ConfigurationGraphAvailable:
            return ConfigurationRecoveryStatus::RuntimeReady;
        case ConfigurationGraphLoadStatus::UnsupportedNewerConfigurationSchema:
            return ConfigurationRecoveryStatus::
                UnsupportedNewerConfigurationSchema;
        case ConfigurationGraphLoadStatus::RootCapacityError:
        case ConfigurationGraphLoadStatus::RecordCapacityError:
            return ConfigurationRecoveryStatus::PersistenceCapacityFailure;
        case ConfigurationGraphLoadStatus::RootReadError:
        case ConfigurationGraphLoadStatus::RecordReadError:
            return ConfigurationRecoveryStatus::PersistenceReadFailure;
        case ConfigurationGraphLoadStatus::ConfigurationGraphIntegrityFailure:
            return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
        case ConfigurationGraphLoadStatus::ConfigurationGraphUnavailable:
        case ConfigurationGraphLoadStatus::
            ConfigurationGraphUnavailableOtherEpoch:
            return ConfigurationRecoveryStatus::ConfigurationUnavailable;
    }
    return ConfigurationRecoveryStatus::ConfigurationUnavailable;
}

ConfigurationRecoveryResult makeResult(
    ConfigurationRecoveryStatus status,
    ConfigurationGraphDiagnostics diagnostics = {}) {
    return {status, diagnostics};
}

ConfigurationRecoveryResult makeUnavailableResult(
    ConfigurationRecoveryStatus status,
    ConfigurationGraphDiagnostics diagnostics = {}) {
    auto result = makeResult(status, diagnostics);
    result.safetyProducer =
        ConfigurationSafetyProducer::ConfigurationUnavailable;
    return result;
}

ConfigurationRecoveryResult makeRejectedWithValidRuntime(
    ConfigurationRecoveryStatus status,
    ConfigurationGraphDiagnostics diagnostics = {}) {
    auto result = makeResult(status, diagnostics);
    result.safetyProducer.reset();
    return result;
}

ConfigurationRecoveryStatus mapBootstrapScanFailure(
    ConfigurationBootstrapScanStatus status) {
    switch (status) {
        case ConfigurationBootstrapScanStatus::ReadError:
            return ConfigurationRecoveryStatus::PersistenceReadFailure;
        case ConfigurationBootstrapScanStatus::CapacityError:
            return ConfigurationRecoveryStatus::PersistenceCapacityFailure;
        default:
            return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
    }
}

ConfigurationRecoveryStatus mapRecoveryBeginStatus(
    ConfigurationRecoveryBeginStatus status) {
    switch (status) {
        case ConfigurationRecoveryBeginStatus::Success:
            return ConfigurationRecoveryStatus::RuntimeReady;
        case ConfigurationRecoveryBeginStatus::ConfigurationModelBudgetBusy:
            return ConfigurationRecoveryStatus::ConfigurationModelBudgetBusy;
        case ConfigurationRecoveryBeginStatus::CounterOverflow:
            return ConfigurationRecoveryStatus::CounterOverflow;
        case ConfigurationRecoveryBeginStatus::StateTransitionRejected:
            return ConfigurationRecoveryStatus::StateTransitionRejected;
    }
    return ConfigurationRecoveryStatus::StateTransitionRejected;
}

bool isBootstrapIndeterminate(ConfigurationBootstrapWriteStatus status) {
    return status ==
           ConfigurationBootstrapWriteStatus::BootstrapCommitIndeterminate;
}

ConfigurationRecoveryStatus mapBootstrapWriteFailure(
    ConfigurationBootstrapWriteStatus status) {
    switch (status) {
        case ConfigurationBootstrapWriteStatus::ReadError:
            return ConfigurationRecoveryStatus::PersistenceReadFailure;
        case ConfigurationBootstrapWriteStatus::CapacityError:
        case ConfigurationBootstrapWriteStatus::WriteCapacityError:
            return ConfigurationRecoveryStatus::PersistenceCapacityFailure;
        case ConfigurationBootstrapWriteStatus::UnsupportedNewerSchema:
            return ConfigurationRecoveryStatus::
                UnsupportedNewerConfigurationSchema;
        case ConfigurationBootstrapWriteStatus::IntegrityFailure:
            return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
        case ConfigurationBootstrapWriteStatus::CounterOverflow:
            return ConfigurationRecoveryStatus::CounterOverflow;
        case ConfigurationBootstrapWriteStatus::BootstrapCommitIndeterminate:
            return ConfigurationRecoveryStatus::BootstrapCommitIndeterminate;
        case ConfigurationBootstrapWriteStatus::InvalidTransition:
            return ConfigurationRecoveryStatus::StateTransitionRejected;
        case ConfigurationBootstrapWriteStatus::WriteError:
        case ConfigurationBootstrapWriteStatus::CommitNotEffective:
        case ConfigurationBootstrapWriteStatus::Success:
            return ConfigurationRecoveryStatus::PersistenceWriteFailure;
    }
    return ConfigurationRecoveryStatus::PersistenceWriteFailure;
}

bool bootstrapFailureLeavesOldState(ConfigurationBootstrapWriteStatus status) {
    return status == ConfigurationBootstrapWriteStatus::ReadError ||
           status == ConfigurationBootstrapWriteStatus::CapacityError ||
           status == ConfigurationBootstrapWriteStatus::WriteError ||
           status == ConfigurationBootstrapWriteStatus::WriteCapacityError ||
           status == ConfigurationBootstrapWriteStatus::CommitNotEffective ||
           status == ConfigurationBootstrapWriteStatus::InvalidTransition ||
           status == ConfigurationBootstrapWriteStatus::CounterOverflow;
}

ConfigurationRecoveryResult makeResetPreparationFailure(
    ConfigurationRecoveryStatus status, bool oldRuntimeRemainsValid) {
    if (oldRuntimeRemainsValid) {
        return makeRejectedWithValidRuntime(status);
    }
    return makeUnavailableResult(status);
}

ConfigurationRecoveryResult makeResetBootstrapFailure(
    ConfigurationBootstrapWriteStatus status, bool oldRuntimeRemainsValid) {
    const auto mapped = mapBootstrapWriteFailure(status);
    if (oldRuntimeRemainsValid && bootstrapFailureLeavesOldState(status)) {
        return makeRejectedWithValidRuntime(mapped);
    }
    return makeUnavailableResult(mapped);
}

bool isResetEligibleNoRuntimeGraph(const ConfigurationGraphLoadResult& graph) {
    if (graph.status ==
        ConfigurationGraphLoadStatus::ConfigurationGraphUnavailable) {
        return true;
    }
    return graph.status == ConfigurationGraphLoadStatus::
                               ConfigurationGraphIntegrityFailure &&
           !graph.diagnostics.globalScanBlocker &&
           !graph.diagnostics.persistentIdentityCollision;
}

}  // namespace

std::unique_ptr<ConfigurationRecoveryService>
ConfigurationRecoveryService::create(
    device_platform::IStateStore& store,
    ConfigurationBootstrapStore& bootstrapStore,
    ConfigurationGraphStore& graphStore,
    ConfigurationService& configurationService,
    ConfigurationMutationCoordinator& mutationCoordinator) {
    if (bootstrapStore.storeIdentity() != &store ||
        graphStore.storeIdentity() != &store ||
        configurationService.graphStoreIdentity() != &graphStore ||
        configurationService.mutationCoordinatorIdentity() !=
            &mutationCoordinator ||
        configurationService.timeZoneResolverIdentity() !=
            graphStore.timeZoneResolverIdentity()) {
        return nullptr;
    }
    return std::unique_ptr<ConfigurationRecoveryService>(
        new ConfigurationRecoveryService(store, bootstrapStore, graphStore,
                                         configurationService,
                                         mutationCoordinator));
}

ConfigurationRecoveryStatus ConfigurationRecoveryService::verifyFactoryEmpty()
    const {
    // The bootstrap scan already read cb0/cb1 under the same mutation lease.
    // These are exactly the remaining 17 known configuration keys.
    const auto inspect = [this](const char* keyValue) {
        const auto read = store_.read(
            key(keyValue),
            configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U);
        if (read.status == device_platform::StateStoreReadStatus::ReadError) {
            return ConfigurationRecoveryStatus::PersistenceReadFailure;
        }
        if (read.status ==
            device_platform::StateStoreReadStatus::CapacityError) {
            return ConfigurationRecoveryStatus::PersistenceCapacityFailure;
        }
        if (read.status != device_platform::StateStoreReadStatus::NotFound) {
            return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
        }
        return ConfigurationRecoveryStatus::RuntimeReady;
    };
    const auto inspectGroup = [&inspect](const auto& keys) {
        for (const auto* keyValue : keys) {
            const auto status = inspect(keyValue);
            if (status != ConfigurationRecoveryStatus::RuntimeReady) {
                return status;
            }
        }
        return ConfigurationRecoveryStatus::RuntimeReady;
    };
    const auto statuses = {
        inspectGroup(
            configuration_storage_contract::kConfigurationRootSlotKeys),
        inspectGroup(
            configuration_storage_contract::kConfigurationManifestSlotKeys),
        inspectGroup(
            configuration_storage_contract::kUserConfigurationSlotKeys),
        inspectGroup(
            configuration_storage_contract::kServiceConfigurationSlotKeys),
        inspectGroup(configuration_storage_contract::kProgramCatalogSlotKeys)};
    for (const auto status : statuses) {
        if (status != ConfigurationRecoveryStatus::RuntimeReady) {
            return status;
        }
    }
    return ConfigurationRecoveryStatus::RuntimeReady;
}

ConfigurationRecoveryResult ConfigurationRecoveryService::mapBootstrapFailure(
    ConfigurationBootstrapScanStatus status) {
    switch (status) {
        case ConfigurationBootstrapScanStatus::ReadError:
            return makeResult(
                ConfigurationRecoveryStatus::PersistenceReadFailure);
        case ConfigurationBootstrapScanStatus::CapacityError:
            return makeResult(
                ConfigurationRecoveryStatus::PersistenceCapacityFailure);
        case ConfigurationBootstrapScanStatus::UnsupportedNewerSchema:
            return {ConfigurationRecoveryStatus::
                        UnsupportedNewerConfigurationSchema,
                    {}};
        case ConfigurationBootstrapScanStatus::IntegrityFailure:
            return {ConfigurationRecoveryStatus::ConfigurationIntegrityFailure,
                    {}};
        case ConfigurationBootstrapScanStatus::Empty:
        case ConfigurationBootstrapScanStatus::Available:
            break;
    }
    return {ConfigurationRecoveryStatus::ConfigurationUnavailable, {}};
}

ConfigurationRecoveryResult
ConfigurationRecoveryService::finalizePublishedGraph(
    const LoadedConfigurationBootstrap& bootstrap,
    const PreparedInitialConfigurationGraph& prepared,
    ConfigurationRecoveryStatus successStatus) {
    const auto finalized = bootstrapStore_.writeSuccessor(
        bootstrap, ConfigurationBootstrapState::Initialized);
    if (finalized.status == ConfigurationBootstrapWriteStatus::Success &&
        finalized.loaded.has_value()) {
        if (finalized.loaded->record.storageEpoch !=
                prepared.graph.active.manifestReference.storageEpoch ||
            !configurationService_.finalizeRecoveredGraph(
                prepared.graph.active.manifestReference.storageEpoch,
                prepared.rootRecordBytes, prepared.planIdentity)) {
            configurationService_.failRecovery(
                ConfigurationRuntimeFailureCause::
                    ServiceStateInvariantViolation);
            return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
        }
        return {successStatus, {}};
    }
    if (isBootstrapIndeterminate(finalized.status)) {
        return {ConfigurationRecoveryStatus::BootstrapCommitIndeterminate, {}};
    }
    return makeUnavailableResult(mapBootstrapWriteFailure(finalized.status));
}

ConfigurationRecoveryResult ConfigurationRecoveryService::continueEpochBuild(
    const LoadedConfigurationBootstrap& bootstrap,
    PreparedInitialConfigurationGraph&& prepared, ChangeOperation operation,
    ConfigurationRecoveryStatus successStatus,
    ConfigurationMutationLease& lease) {
    if (!configurationService_.prepareRecoveredGraph(prepared.graph)) {
        return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
    }
    const auto reboundBootstrap = bootstrapStore_.scan();
    if (reboundBootstrap.status !=
            ConfigurationBootstrapScanStatus::Available ||
        !reboundBootstrap.loaded.has_value() ||
        reboundBootstrap.loaded->slot != bootstrap.slot ||
        reboundBootstrap.loaded->record != bootstrap.record ||
        reboundBootstrap.loaded->canonicalRecordBytes !=
            bootstrap.canonicalRecordBytes) {
        configurationService_.failRecovery(
            ConfigurationRuntimeFailureCause::PersistentGraphIntegrityFailure);
        return makeResult(mapBootstrapScanFailure(reboundBootstrap.status));
    }
    const auto stateRevision = configurationService_.stateRevision();
    const auto recoveryGeneration = configurationService_.recoveryGeneration();
    ConfigurationEpochGraphWriteCapability capability(
        bootstrap, stateRevision, recoveryGeneration, prepared.planIdentity,
        prepared, lease);
    if (!configurationService_.validateRecoveryBinding(stateRevision,
                                                       recoveryGeneration)) {
        return makeResult(ConfigurationRecoveryStatus::StateTransitionRejected);
    }
    const auto execution =
        graphStore_.executeInitialGraph(prepared, capability);
    lastResourcePeaks_ = ConfigurationRecoveryResourcePeaks{
        prepared.peakProgramPayloadCapacity,
        prepared.peakDocumentEnvelopeCapacity,
        prepared.peakStoreReadbackCapacity,
        prepared.smallCanonicalRecordCapacity,
        configurationService_.fullModelGenerationCount()};
    if (execution.status == ConfigurationCommitExecutionStatus::Activated) {
        CommittedRecoveryActivation activation(
            prepared.graph.active.manifestReference.storageEpoch,
            prepared.graph.rootSlot, prepared.rootRecordBytes,
            prepared.planIdentity, configurationService_.stateRevision(),
            recoveryGeneration, prepared.graph);
        if (!configurationService_.publishRecoveredGraph(
                std::move(activation))) {
            return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
        }
        return finalizePublishedGraph(bootstrap, prepared, successStatus);
    }
    if (execution.status ==
        ConfigurationCommitExecutionStatus::CommitIndeterminate) {
        if (!configurationService_.transitionRecovery(
                ConfigurationServiceMode::CommitIndeterminate)) {
            configurationService_.failRecovery(
                ConfigurationRuntimeFailureCause::
                    ServiceStateInvariantViolation);
            return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
        }
        pendingRoot_ = PendingRootResolution{
            std::move(prepared), bootstrap,
            operation,           successStatus,
            recoveryGeneration,  configurationService_.stateRevision()};
        return {ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate,
                {}};
    }
    if (execution.status ==
        ConfigurationCommitExecutionStatus::RuntimeFailure) {
        configurationService_.failRecovery(
            ConfigurationRuntimeFailureCause::PersistentGraphIntegrityFailure);
        return {ConfigurationRecoveryStatus::ConfigurationIntegrityFailure, {}};
    }
    const auto resumeMode =
        bootstrap.record.state == ConfigurationBootstrapState::Resetting
            ? ConfigurationServiceMode::EpochResetting
            : ConfigurationServiceMode::RecoveryPreparing;
    if (!configurationService_.discardPreparedRecovery(resumeMode)) {
        return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
    }
    auto status = ConfigurationRecoveryStatus::PersistenceWriteFailure;
    if (execution.status ==
        ConfigurationCommitExecutionStatus::CapacityFailure) {
        status = ConfigurationRecoveryStatus::PersistenceCapacityFailure;
    } else if (execution.status ==
               ConfigurationCommitExecutionStatus::RecordOutcomeIndeterminate) {
        status = ConfigurationRecoveryStatus::
            ConfigurationRecordOutcomeIndeterminate;
    }
    return {status, {}};
}

ConfigurationRecoveryResult ConfigurationRecoveryService::resolvePendingRoot(
    ConfigurationMutationLease& lease) {
    if (!pendingRoot_.has_value() || !lease.valid()) {
        return makeResult(
            ConfigurationRecoveryStatus::ConfigurationUnavailable);
    }
    if (!configurationService_.validateRecoveryBinding(
            pendingRoot_->serviceStateRevision,
            pendingRoot_->recoveryGeneration)) {
        configurationService_.failRecovery(
            ConfigurationRuntimeFailureCause::PublishContractViolation);
        return makeResult(
            ConfigurationRecoveryStatus::ConfigurationIntegrityFailure);
    }
    const auto resolution =
        graphStore_.resolveInitialGraph(pendingRoot_->prepared);
    if (resolution.status ==
        ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate) {
        return makeResult(
            ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate);
    }
    if (resolution.status ==
        ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure) {
        pendingRoot_.reset();
        configurationService_.failRecovery(
            ConfigurationRuntimeFailureCause::PersistentGraphIntegrityFailure);
        return makeResult(
            ConfigurationRecoveryStatus::ConfigurationIntegrityFailure);
    }
    if (resolution.status ==
        ConfigurationCommitResolutionStatus::ResolutionRecoveredOld) {
        const auto resumeMode =
            pendingRoot_->bootstrap.record.state ==
                    ConfigurationBootstrapState::Resetting
                ? ConfigurationServiceMode::EpochResetting
                : ConfigurationServiceMode::RecoveryPreparing;
        pendingRoot_.reset();
        if (!configurationService_.discardPreparedRecovery(resumeMode)) {
            return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
        }
        return makeResult(
            ConfigurationRecoveryStatus::ConfigurationUnavailable);
    }
    auto completed = std::move(*pendingRoot_);
    pendingRoot_.reset();
    auto loaded =
        graphStore_.loadCanonicalGraph(completed.bootstrap.record.storageEpoch);
    if (loaded.status !=
            ConfigurationGraphLoadStatus::ConfigurationGraphAvailable ||
        !loaded.graph.has_value() ||
        loaded.graph->canonicalRootRecordBytes !=
            completed.prepared.rootRecordBytes) {
        configurationService_.failRecovery(
            ConfigurationRuntimeFailureCause::PersistentGraphIntegrityFailure);
        return makeResult(mapLoad(loaded.status), loaded.diagnostics);
    }
    completed.prepared.graph = std::move(*loaded.graph);
    CommittedRecoveryActivation activation(
        completed.prepared.graph.active.manifestReference.storageEpoch,
        completed.prepared.graph.rootSlot, completed.prepared.rootRecordBytes,
        completed.prepared.planIdentity, configurationService_.stateRevision(),
        completed.recoveryGeneration, completed.prepared.graph);
    if (!configurationService_.publishRecoveredGraph(std::move(activation))) {
        return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
    }
    return finalizePublishedGraph(completed.bootstrap, completed.prepared,
                                  completed.successStatus);
}

// Boot keeps the persistent-state classification and its recovery transitions
// in one auditable decision path.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
ConfigurationRecoveryResult ConfigurationRecoveryService::boot() {
    auto acquired = mutationCoordinator_.tryAcquire();
    if (acquired.status != ConfigurationMutationAcquireStatus::Acquired) {
        return {ConfigurationRecoveryStatus::ConfigurationMutationBusy, {}};
    }
    if (pendingRoot_.has_value()) {
        return resolvePendingRoot(acquired.lease);
    }

    auto bootstrap = bootstrapStore_.scan();
    if (configurationService_.mode() ==
        ConfigurationServiceMode::BootstrapFinalizationPending) {
        if (bootstrap.status == ConfigurationBootstrapScanStatus::Available &&
            bootstrap.loaded.has_value()) {
            if (bootstrap.loaded->record.state ==
                ConfigurationBootstrapState::Initialized) {
                if (configurationService_.finalizeRecoveredGraphForBootstrap(
                        bootstrap.loaded->record.storageEpoch)) {
                    return {ConfigurationRecoveryStatus::RuntimeReady, {}};
                }
            } else {
                const auto successStatus =
                    bootstrap.loaded->record.state ==
                            ConfigurationBootstrapState::Initializing
                        ? ConfigurationRecoveryStatus::
                              FactoryInitializationCompleted
                        : ConfigurationRecoveryStatus::FactoryResetCompleted;
                const auto finalized = bootstrapStore_.writeSuccessor(
                    *bootstrap.loaded,
                    ConfigurationBootstrapState::Initialized);
                if (finalized.status ==
                        ConfigurationBootstrapWriteStatus::Success &&
                    finalized.loaded.has_value() &&
                    configurationService_.finalizeRecoveredGraphForBootstrap(
                        finalized.loaded->record.storageEpoch)) {
                    return makeResult(successStatus);
                }
                return makeResult(
                    isBootstrapIndeterminate(finalized.status)
                        ? ConfigurationRecoveryStatus::
                              BootstrapCommitIndeterminate
                        : ConfigurationRecoveryStatus::PersistenceWriteFailure);
            }
        }
        return mapBootstrapFailure(bootstrap.status);
    }

    if (bootstrap.status == ConfigurationBootstrapScanStatus::Empty) {
        const auto empty = verifyFactoryEmpty();
        if (empty != ConfigurationRecoveryStatus::RuntimeReady) {
            return {empty, {}};
        }
        const auto begin = configurationService_.beginRecovery(
            ConfigurationServiceMode::RecoveryPreparing,
            configuration_limits::kInitializationRecoveryRevisionHeadroom);
        if (begin != ConfigurationRecoveryBeginStatus::Success) {
            return makeUnavailableResult(mapRecoveryBeginStatus(begin));
        }
        auto prepared = graphStore_.prepareInitialGraph(
            device_platform::StorageEpoch{1U}, decodeChangeOperation(2U));
        if (prepared.status != InitialConfigurationPrepareStatus::Success ||
            !prepared.prepared.has_value()) {
            static_cast<void>(configurationService_.cancelRecovery());
            return makeResult(mapPrepare(prepared.status));
        }
        auto initial = bootstrapStore_.writeInitialInitializing();
        if (initial.status != ConfigurationBootstrapWriteStatus::Success ||
            !initial.loaded.has_value()) {
            if (isBootstrapIndeterminate(initial.status)) {
                configurationService_.failRecovery(
                    ConfigurationRuntimeFailureCause::
                        PersistentStoreReadFailure);
                return {
                    ConfigurationRecoveryStatus::BootstrapCommitIndeterminate,
                    {}};
            }
            static_cast<void>(configurationService_.cancelRecovery());
            return makeUnavailableResult(
                mapBootstrapWriteFailure(initial.status));
        }
        return continueEpochBuild(
            *initial.loaded, std::move(*prepared.prepared),
            decodeChangeOperation(2U),
            ConfigurationRecoveryStatus::FactoryInitializationCompleted,
            acquired.lease);
    }
    if (bootstrap.status != ConfigurationBootstrapScanStatus::Available ||
        !bootstrap.loaded.has_value()) {
        return mapBootstrapFailure(bootstrap.status);
    }
    if (bootstrap.loaded->record.state ==
        ConfigurationBootstrapState::Initialized) {
        auto loaded = graphStore_.loadCanonicalGraph(
            bootstrap.loaded->record.storageEpoch);
        if (loaded.status !=
                ConfigurationGraphLoadStatus::ConfigurationGraphAvailable ||
            !loaded.graph.has_value()) {
            const auto status = mapLoad(loaded.status);
            if ((status ==
                     ConfigurationRecoveryStatus::ConfigurationUnavailable ||
                 status == ConfigurationRecoveryStatus::
                               ConfigurationIntegrityFailure) &&
                !loaded.diagnostics.globalScanBlocker &&
                !loaded.diagnostics.persistentIdentityCollision &&
                configurationService_.mode() ==
                    ConfigurationServiceMode::NoRuntime) {
                static_cast<void>(
                    configurationService_.markResetEligibleNoRuntime());
            }
            return makeResult(status, loaded.diagnostics);
        }
        const auto begin = configurationService_.beginRecovery(
            ConfigurationServiceMode::RecoveryPreparing,
            configuration_limits::kNormalBootRevisionHeadroom);
        if (begin != ConfigurationRecoveryBeginStatus::Success) {
            return makeUnavailableResult(mapRecoveryBeginStatus(begin),
                                         loaded.diagnostics);
        }
        if (!configurationService_.prepareRecoveredGraph(*loaded.graph)) {
            return makeUnavailableResult(
                ConfigurationRecoveryStatus::RuntimePreparationFailure,
                loaded.diagnostics);
        }
        const auto planIdentity = device_platform::computeCrc32IsoHdlc(
            loaded.graph->canonicalRootRecordBytes);
        CommittedRecoveryActivation activation(
            bootstrap.loaded->record.storageEpoch, loaded.graph->rootSlot,
            loaded.graph->canonicalRootRecordBytes, planIdentity,
            configurationService_.stateRevision(),
            configurationService_.recoveryGeneration(), *loaded.graph);
        if (!configurationService_.publishRecoveredGraph(
                std::move(activation)) ||
            !configurationService_.finalizeRecoveredGraph(
                bootstrap.loaded->record.storageEpoch,
                loaded.graph->canonicalRootRecordBytes, planIdentity)) {
            return makeResult(
                ConfigurationRecoveryStatus::RuntimePreparationFailure,
                loaded.diagnostics);
        }
        return {ConfigurationRecoveryStatus::RuntimeReady, loaded.diagnostics};
    }
    const auto operation = bootstrap.loaded->record.state ==
                                   ConfigurationBootstrapState::Initializing
                               ? decodeChangeOperation(2U)
                               : decodeChangeOperation(5U);
    const auto mode =
        bootstrap.loaded->record.state == ConfigurationBootstrapState::Resetting
            ? ConfigurationServiceMode::EpochResetting
            : ConfigurationServiceMode::RecoveryPreparing;
    const auto begin = configurationService_.beginRecovery(
        mode, configuration_limits::kInitializationRecoveryRevisionHeadroom);
    if (begin != ConfigurationRecoveryBeginStatus::Success) {
        return makeResult(mapRecoveryBeginStatus(begin));
    }
    auto prepared = graphStore_.prepareInitialGraph(
        bootstrap.loaded->record.storageEpoch, operation);
    if (prepared.status != InitialConfigurationPrepareStatus::Success ||
        !prepared.prepared.has_value()) {
        static_cast<void>(configurationService_.cancelRecovery());
        return makeResult(mapPrepare(prepared.status));
    }
    return continueEpochBuild(
        *bootstrap.loaded, std::move(*prepared.prepared), operation,
        bootstrap.loaded->record.state ==
                ConfigurationBootstrapState::Initializing
            ? ConfigurationRecoveryStatus::FactoryInitializationCompleted
            : ConfigurationRecoveryStatus::FactoryResetCompleted,
        acquired.lease);
}

ConfigurationRecoveryResult
ConfigurationRecoveryService::beginAuthorizedFactoryReset() {
    auto acquired = mutationCoordinator_.tryAcquire();
    if (acquired.status != ConfigurationMutationAcquireStatus::Acquired) {
        return {ConfigurationRecoveryStatus::ConfigurationMutationBusy, {}};
    }
    auto bootstrap = bootstrapStore_.scan();
    if (bootstrap.status != ConfigurationBootstrapScanStatus::Available ||
        !bootstrap.loaded.has_value() ||
        bootstrap.loaded->record.state !=
            ConfigurationBootstrapState::Initialized) {
        return bootstrap.status == ConfigurationBootstrapScanStatus::Available
                   ? ConfigurationRecoveryResult{ConfigurationRecoveryStatus::
                                                     ConfigurationUnavailable,
                                                 {}}
                   : mapBootstrapFailure(bootstrap.status);
    }
    constexpr auto kMaximumResettableEpoch =
        std::numeric_limits<std::uint64_t>::max() / 2U - 1U;
    if (bootstrap.loaded->record.storageEpoch.value() >
            kMaximumResettableEpoch ||
        bootstrap.loaded->highWater.value() ==
            std::numeric_limits<std::uint64_t>::max()) {
        return makeResult(ConfigurationRecoveryStatus::CounterOverflow);
    }
    if (configurationService_.mode() == ConfigurationServiceMode::NoRuntime) {
        const auto graph = graphStore_.loadCanonicalGraph(
            bootstrap.loaded->record.storageEpoch);
        if (!isResetEligibleNoRuntimeGraph(graph)) {
            return makeResult(mapLoad(graph.status), graph.diagnostics);
        }
        if (!configurationService_.markResetEligibleNoRuntime()) {
            return makeResult(
                ConfigurationRecoveryStatus::StateTransitionRejected);
        }
    }
    if (configurationService_.mode() != ConfigurationServiceMode::Operational &&
        configurationService_.mode() !=
            ConfigurationServiceMode::ResetEligibleNoRuntime) {
        return makeResult(ConfigurationRecoveryStatus::StateTransitionRejected);
    }
    const auto targetEpoch = device_platform::StorageEpoch{
        bootstrap.loaded->record.storageEpoch.value() + 1U};
    const bool oldRuntimeRemainsValid =
        configurationService_.mode() == ConfigurationServiceMode::Operational;
    const auto begin = configurationService_.beginRecovery(
        ConfigurationServiceMode::ResetPreparing,
        configuration_limits::kResetRecoveryRevisionHeadroom);
    if (begin != ConfigurationRecoveryBeginStatus::Success) {
        return makeResult(mapRecoveryBeginStatus(begin));
    }
    auto prepared =
        graphStore_.prepareInitialGraph(targetEpoch, decodeChangeOperation(5U));
    if (prepared.status != InitialConfigurationPrepareStatus::Success ||
        !prepared.prepared.has_value()) {
        static_cast<void>(configurationService_.cancelRecovery());
        return makeResetPreparationFailure(mapPrepare(prepared.status),
                                           oldRuntimeRemainsValid);
    }
    auto resetting = bootstrapStore_.writeSuccessor(
        *bootstrap.loaded, ConfigurationBootstrapState::Resetting);
    if (resetting.status != ConfigurationBootstrapWriteStatus::Success ||
        !resetting.loaded.has_value()) {
        if (isBootstrapIndeterminate(resetting.status)) {
            configurationService_.failRecovery(
                ConfigurationRuntimeFailureCause::PersistentStoreReadFailure);
            return {ConfigurationRecoveryStatus::BootstrapCommitIndeterminate,
                    {}};
        }
        static_cast<void>(configurationService_.cancelRecovery());
        return makeResetBootstrapFailure(resetting.status,
                                         oldRuntimeRemainsValid);
    }
    if (!configurationService_.transitionRecovery(
            ConfigurationServiceMode::EpochResetting)) {
        return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
    }
    return continueEpochBuild(
        *resetting.loaded, std::move(*prepared.prepared),
        decodeChangeOperation(5U),
        ConfigurationRecoveryStatus::FactoryResetCompleted, acquired.lease);
}

}  // namespace fermentation
