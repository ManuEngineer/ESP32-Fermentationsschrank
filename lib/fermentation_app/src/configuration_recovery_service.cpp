#include "configuration_recovery_service.hpp"

#include <array>
#include <limits>
#include <utility>

#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
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
            return ConfigurationRecoveryStatus::CapacityFailure;
        case InitialConfigurationPrepareStatus::PersistenceFailure:
            return ConfigurationRecoveryStatus::PersistenceFailure;
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
            return ConfigurationRecoveryStatus::CapacityFailure;
        case ConfigurationGraphLoadStatus::RootReadError:
        case ConfigurationGraphLoadStatus::RecordReadError:
            return ConfigurationRecoveryStatus::PersistenceFailure;
        case ConfigurationGraphLoadStatus::ConfigurationGraphIntegrityFailure:
            return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
        case ConfigurationGraphLoadStatus::ConfigurationGraphUnavailable:
        case ConfigurationGraphLoadStatus::
            ConfigurationGraphUnavailableOtherEpoch:
            return ConfigurationRecoveryStatus::ConfigurationUnavailable;
    }
    return ConfigurationRecoveryStatus::ConfigurationUnavailable;
}

bool isBootstrapIndeterminate(ConfigurationBootstrapWriteStatus status) {
    return status ==
           ConfigurationBootstrapWriteStatus::BootstrapCommitIndeterminate;
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
    const std::array<const char*, 17> keys{
        "cr0", "cr1", "cm0", "cm1", "cm2", "uc0", "uc1", "uc2", "uc3",
        "sc0", "sc1", "sc2", "sc3", "pc0", "pc1", "pc2", "pc3"};
    for (const auto* keyValue : keys) {
        const auto read = store_.read(
            key(keyValue),
            configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U);
        if (read.status == device_platform::StateStoreReadStatus::ReadError) {
            return ConfigurationRecoveryStatus::PersistenceFailure;
        }
        if (read.status ==
            device_platform::StateStoreReadStatus::CapacityError) {
            return ConfigurationRecoveryStatus::CapacityFailure;
        }
        if (read.status != device_platform::StateStoreReadStatus::NotFound) {
            return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
        }
    }
    return ConfigurationRecoveryStatus::RuntimeReady;
}

ConfigurationRecoveryResult ConfigurationRecoveryService::mapBootstrapFailure(
    ConfigurationBootstrapScanStatus status) {
    switch (status) {
        case ConfigurationBootstrapScanStatus::ReadError:
            return {ConfigurationRecoveryStatus::PersistenceFailure, {}};
        case ConfigurationBootstrapScanStatus::CapacityError:
            return {ConfigurationRecoveryStatus::CapacityFailure, {}};
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
    ConfigurationRecoveryStatus successStatus) {
    const auto finalized = bootstrapStore_.writeSuccessor(
        bootstrap, ConfigurationBootstrapState::Initialized);
    if (finalized.status == ConfigurationBootstrapWriteStatus::Success &&
        finalized.loaded.has_value()) {
        if (!configurationService_.finalizeRecoveredGraph()) {
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
    configurationService_.failRecovery(
        ConfigurationRuntimeFailureCause::PersistentGraphVerificationFailure);
    return {finalized.status ==
                    ConfigurationBootstrapWriteStatus::WriteCapacityError
                ? ConfigurationRecoveryStatus::CapacityFailure
                : ConfigurationRecoveryStatus::PersistenceFailure,
            {}};
}

ConfigurationRecoveryResult ConfigurationRecoveryService::continueEpochBuild(
    const LoadedConfigurationBootstrap& bootstrap,
    PreparedInitialConfigurationGraph&& prepared, ChangeOperation operation,
    ConfigurationRecoveryStatus successStatus,
    ConfigurationMutationLease& lease) {
    if (!configurationService_.prepareRecoveredGraph(prepared.graph)) {
        return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
    }
    ConfigurationEpochGraphWriteCapability capability(
        bootstrap.record.storageEpoch, bootstrap.slot, bootstrap.record.state,
        prepared.planIdentity, prepared, lease);
    const auto execution =
        graphStore_.executeInitialGraph(prepared, capability);
    if (execution.status == ConfigurationCommitExecutionStatus::Activated) {
        if (!configurationService_.publishRecoveredGraph()) {
            return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
        }
        return finalizePublishedGraph(bootstrap, successStatus);
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
        pendingRoot_ = PendingRootResolution{std::move(prepared), bootstrap,
                                             operation, successStatus};
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
    auto status = ConfigurationRecoveryStatus::PersistenceFailure;
    if (execution.status ==
        ConfigurationCommitExecutionStatus::CapacityFailure) {
        status = ConfigurationRecoveryStatus::CapacityFailure;
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
        return {ConfigurationRecoveryStatus::ConfigurationUnavailable, {}};
    }
    const auto resolution =
        graphStore_.resolveInitialGraph(pendingRoot_->prepared);
    if (resolution.status ==
        ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate) {
        return {ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate,
                {}};
    }
    if (resolution.status ==
        ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure) {
        pendingRoot_.reset();
        configurationService_.failRecovery(
            ConfigurationRuntimeFailureCause::PersistentGraphIntegrityFailure);
        return {ConfigurationRecoveryStatus::ConfigurationIntegrityFailure, {}};
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
        return {ConfigurationRecoveryStatus::ConfigurationUnavailable, {}};
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
        return {mapLoad(loaded.status), loaded.diagnostics};
    }
    if (!configurationService_.publishRecoveredGraph()) {
        return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
    }
    return finalizePublishedGraph(completed.bootstrap, completed.successStatus);
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
                if (configurationService_.finalizeRecoveredGraph()) {
                    return {ConfigurationRecoveryStatus::RuntimeReady, {}};
                }
            } else {
                return finalizePublishedGraph(
                    *bootstrap.loaded,
                    bootstrap.loaded->record.state ==
                            ConfigurationBootstrapState::Initializing
                        ? ConfigurationRecoveryStatus::
                              FactoryInitializationCompleted
                        : ConfigurationRecoveryStatus::FactoryResetCompleted);
            }
        }
        return mapBootstrapFailure(bootstrap.status);
    }

    if (bootstrap.status == ConfigurationBootstrapScanStatus::Empty) {
        const auto empty = verifyFactoryEmpty();
        if (empty != ConfigurationRecoveryStatus::RuntimeReady) {
            return {empty, {}};
        }
        auto prepared = graphStore_.prepareInitialGraph(
            device_platform::StorageEpoch{1U}, decodeChangeOperation(2U));
        if (prepared.status != InitialConfigurationPrepareStatus::Success ||
            !prepared.prepared.has_value()) {
            return {mapPrepare(prepared.status), {}};
        }
        if (!configurationService_.beginRecovery(
                ConfigurationServiceMode::RecoveryPreparing,
                configuration_limits::
                    kInitializationRecoveryRevisionHeadroom)) {
            return {ConfigurationRecoveryStatus::CounterOverflow, {}};
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
            return {
                initial.status ==
                        ConfigurationBootstrapWriteStatus::WriteCapacityError
                    ? ConfigurationRecoveryStatus::CapacityFailure
                    : ConfigurationRecoveryStatus::PersistenceFailure,
                {}};
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
            return {mapLoad(loaded.status), loaded.diagnostics};
        }
        if (!configurationService_.beginRecovery(
                ConfigurationServiceMode::RecoveryPreparing,
                configuration_limits::kNormalBootRevisionHeadroom) ||
            !configurationService_.prepareRecoveredGraph(*loaded.graph) ||
            !configurationService_.publishRecoveredGraph() ||
            !configurationService_.finalizeRecoveredGraph()) {
            return {ConfigurationRecoveryStatus::RuntimePreparationFailure,
                    loaded.diagnostics};
        }
        return {ConfigurationRecoveryStatus::RuntimeReady, loaded.diagnostics};
    }
    const auto operation = bootstrap.loaded->record.state ==
                                   ConfigurationBootstrapState::Initializing
                               ? decodeChangeOperation(2U)
                               : decodeChangeOperation(5U);
    auto prepared = graphStore_.prepareInitialGraph(
        bootstrap.loaded->record.storageEpoch, operation);
    if (prepared.status != InitialConfigurationPrepareStatus::Success ||
        !prepared.prepared.has_value()) {
        return {mapPrepare(prepared.status), {}};
    }
    const auto mode =
        bootstrap.loaded->record.state == ConfigurationBootstrapState::Resetting
            ? ConfigurationServiceMode::EpochResetting
            : ConfigurationServiceMode::RecoveryPreparing;
    if (!configurationService_.beginRecovery(
            mode,
            configuration_limits::kInitializationRecoveryRevisionHeadroom)) {
        return {ConfigurationRecoveryStatus::CounterOverflow, {}};
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
    if (bootstrap.loaded->record.storageEpoch.value() ==
        std::numeric_limits<std::uint64_t>::max()) {
        return {ConfigurationRecoveryStatus::CounterOverflow, {}};
    }
    const auto targetEpoch = device_platform::StorageEpoch{
        bootstrap.loaded->record.storageEpoch.value() + 1U};
    auto prepared =
        graphStore_.prepareInitialGraph(targetEpoch, decodeChangeOperation(5U));
    if (prepared.status != InitialConfigurationPrepareStatus::Success ||
        !prepared.prepared.has_value()) {
        return {mapPrepare(prepared.status), {}};
    }
    if (!configurationService_.beginRecovery(
            ConfigurationServiceMode::ResetPreparing,
            configuration_limits::kResetRecoveryRevisionHeadroom)) {
        return {ConfigurationRecoveryStatus::CounterOverflow, {}};
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
        return {resetting.status ==
                        ConfigurationBootstrapWriteStatus::WriteCapacityError
                    ? ConfigurationRecoveryStatus::CapacityFailure
                    : ConfigurationRecoveryStatus::PersistenceFailure,
                {}};
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
