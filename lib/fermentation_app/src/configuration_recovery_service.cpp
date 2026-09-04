#include "configuration_recovery_service.hpp"

#include <array>
#include <limits>
#include <new>
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
    // Never demote an already-assigned integrity producer: this call site
    // only asserts "no runtime survives", not which producer applies.
    if (!result.safetyProducer.has_value()) {
        result.safetyProducer =
            ConfigurationSafetyProducer::ConfigurationUnavailable;
    }
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
        case ConfigurationBootstrapScanStatus::UnsupportedNewerSchema:
            return ConfigurationRecoveryStatus::
                UnsupportedNewerConfigurationSchema;
        case ConfigurationBootstrapScanStatus::IntegrityFailure:
        case ConfigurationBootstrapScanStatus::Empty:
        case ConfigurationBootstrapScanStatus::Available:
            return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
    }
    return ConfigurationRecoveryStatus::ConfigurationIntegrityFailure;
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

// A write attempt is provably safe-not-effective only when the store was
// never actually mutated, or the readback proves the prior bytes are still
// exactly in place. InvalidTransition means the canonical bootstrap changed
// underneath this attempt (or the target relation became disallowed) and is
// therefore never proof that the previously bound old state still persists.
bool bootstrapFailureLeavesOldState(ConfigurationBootstrapWriteStatus status) {
    return status == ConfigurationBootstrapWriteStatus::ReadError ||
           status == ConfigurationBootstrapWriteStatus::CapacityError ||
           status == ConfigurationBootstrapWriteStatus::WriteError ||
           status == ConfigurationBootstrapWriteStatus::WriteCapacityError ||
           status == ConfigurationBootstrapWriteStatus::CommitNotEffective ||
           status == ConfigurationBootstrapWriteStatus::CounterOverflow;
}

ConfigurationRuntimeFailureCause mapBootstrapWriteFailureCause(
    ConfigurationBootstrapWriteStatus status) {
    return status == ConfigurationBootstrapWriteStatus::UnsupportedNewerSchema
               ? ConfigurationRuntimeFailureCause::
                     UnsupportedNewerConfigurationSchema
               : ConfigurationRuntimeFailureCause::
                     PersistentGraphIntegrityFailure;
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
        new (std::nothrow) ConfigurationRecoveryService(
            store, bootstrapStore, graphStore, configurationService,
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

// The single outcome matrix for finalizing a published graph's bootstrap
// record to Initialized, shared by the direct publish path
// (finalizePublishedGraph()) and the BootstrapFinalizationPending resume
// path in boot(). finalizeGraph performs the caller-specific graph-side
// finalization (which differs in what binding data is available) and is
// only invoked once the bootstrap write itself is exactly confirmed.
template <typename FinalizeGraphFn>
ConfigurationRecoveryResult
ConfigurationRecoveryService::classifyBootstrapFinalization(
    const ConfigurationBootstrapWriteResult& finalized,
    ConfigurationRecoveryStatus successStatus,
    FinalizeGraphFn&& finalizeGraph) {
    if (finalized.status == ConfigurationBootstrapWriteStatus::Success &&
        finalized.loaded.has_value()) {
        if (!finalizeGraph(finalized.loaded->record.storageEpoch)) {
            configurationService_.failRecovery(
                ConfigurationRuntimeFailureCause::
                    ServiceStateInvariantViolation);
            return {ConfigurationRecoveryStatus::RuntimePreparationFailure, {}};
        }
        if (successStatus ==
            ConfigurationRecoveryStatus::FactoryResetCompleted) {
            armAuthorizedRunEpochHandoff(finalized.loaded->record);
        }
        return {successStatus, {}};
    }
    if (isBootstrapIndeterminate(finalized.status)) {
        return {ConfigurationRecoveryStatus::BootstrapCommitIndeterminate, {}};
    }
    if (bootstrapFailureLeavesOldState(finalized.status)) {
        // BootstrapFinalizationPending is retained: no further write is
        // attempted and no normal runtime is available until a later
        // attempt confirms Initialized.
        return makeUnavailableResult(
            mapBootstrapWriteFailure(finalized.status));
    }
    // Integrity failure, unsupported newer schema or a foreign canonical
    // bootstrap discovered during finalization: the already-published graph
    // is no longer provably bound to a safe persistent state.
    configurationService_.failRecovery(
        mapBootstrapWriteFailureCause(finalized.status));
    return makeUnavailableResult(mapBootstrapWriteFailure(finalized.status));
}

void ConfigurationRecoveryService::armAuthorizedRunEpochHandoff(
    const ConfigurationBootstrapRecord& record) noexcept {
    if (record.schemaVersion != kConfigurationBootstrapSchemaVersion2 ||
        record.state != ConfigurationBootstrapState::Initialized ||
        (record.handoff != RunEpochHandoffState::Pending &&
         record.handoff != RunEpochHandoffState::Committed) ||
        !record.previousEpoch.has_value() || !record.currentEpoch.has_value() ||
        record.currentEpoch.value() != record.storageEpoch ||
        record.previousEpoch->value() == 0U ||
        record.previousEpoch->value() ==
            std::numeric_limits<std::uint64_t>::max() ||
        record.previousEpoch->value() + 1U != record.currentEpoch->value()) {
        pendingRunEpochHandoff_.reset();
        return;
    }
    pendingRunEpochHandoff_ = AuthorizedRunEpochHandoffProof(
        *record.previousEpoch, *record.currentEpoch,
        record.handoff == RunEpochHandoffState::Pending
            ? AuthorizedRunEpochHandoffPhase::Pending
            : AuthorizedRunEpochHandoffPhase::Committed);
}

std::optional<AuthorizedRunEpochHandoffProof>
ConfigurationRecoveryService::takeAuthorizedRunEpochHandoffProof() noexcept {
    auto proof = std::move(pendingRunEpochHandoff_);
    pendingRunEpochHandoff_.reset();
    return proof;
}

ConfigurationRecoveryResult
ConfigurationRecoveryService::commitAuthorizedRunEpochHandoff(
    AuthorizedRunEpochHandoffProof& proof,
    const AuthorizedRunEpochHandoffSlotsPrepared& prepared) {
    if (proof.phase() != AuthorizedRunEpochHandoffPhase::Pending ||
        prepared.previousEpoch() != proof.previousEpoch() ||
        prepared.currentEpoch() != proof.currentEpoch()) {
        return makeUnavailableResult(
            ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable);
    }
    auto acquired = mutationCoordinator_.tryAcquire();
    if (acquired.status != ConfigurationMutationAcquireStatus::Acquired) {
        return makeUnavailableResult(
            ConfigurationRecoveryStatus::ConfigurationMutationBusy);
    }
    auto bootstrap = bootstrapStore_.scan();
    if (bootstrap.status != ConfigurationBootstrapScanStatus::Available ||
        !bootstrap.loaded.has_value() ||
        bootstrap.loaded->record.schemaVersion !=
            kConfigurationBootstrapSchemaVersion2 ||
        bootstrap.loaded->record.state !=
            ConfigurationBootstrapState::Initialized ||
        !bootstrap.loaded->record.previousEpoch.has_value() ||
        !bootstrap.loaded->record.currentEpoch.has_value() ||
        bootstrap.loaded->record.previousEpoch.value() !=
            proof.previousEpoch() ||
        bootstrap.loaded->record.currentEpoch.value() != proof.currentEpoch()) {
        return mapBootstrapFailure(bootstrap.status);
    }

    auto current = *bootstrap.loaded;
    if (current.record.handoff == RunEpochHandoffState::Committed) {
        proof.promoteToCommitted();
        return {ConfigurationRecoveryStatus::RuntimeReady, {}};
    }
    if (current.record.handoff != RunEpochHandoffState::Pending) {
        return current.record.handoff == RunEpochHandoffState::Consumed
                   ? makeRejectedWithValidRuntime(
                         ConfigurationRecoveryStatus::StateTransitionRejected)
                   : makeUnavailableResult(ConfigurationRecoveryStatus::
                                               ConfigurationIntegrityFailure);
    }
    const auto committed = bootstrapStore_.writeHandoffSuccessor(
        current, RunEpochHandoffState::Committed);
    if (committed.status != ConfigurationBootstrapWriteStatus::Success ||
        !committed.loaded.has_value()) {
        return makeUnavailableResult(
            mapBootstrapWriteFailure(committed.status));
    }
    proof.promoteToCommitted();
    return {ConfigurationRecoveryStatus::RuntimeReady, {}};
}

ConfigurationRecoveryResult
ConfigurationRecoveryService::consumeAuthorizedRunEpochHandoff(
    AuthorizedRunEpochHandoffProof& proof,
    const AuthorizedRunEpochHandoffHeadFinalized& finalized) {
    if (proof.phase() != AuthorizedRunEpochHandoffPhase::Committed ||
        finalized.previousEpoch() != proof.previousEpoch() ||
        finalized.currentEpoch() != proof.currentEpoch()) {
        return makeUnavailableResult(
            ConfigurationRecoveryStatus::RunPersistenceHandoffUnavailable);
    }
    auto acquired = mutationCoordinator_.tryAcquire();
    if (acquired.status != ConfigurationMutationAcquireStatus::Acquired) {
        return makeUnavailableResult(
            ConfigurationRecoveryStatus::ConfigurationMutationBusy);
    }
    auto bootstrap = bootstrapStore_.scan();
    if (bootstrap.status != ConfigurationBootstrapScanStatus::Available ||
        !bootstrap.loaded.has_value() ||
        bootstrap.loaded->record.schemaVersion !=
            kConfigurationBootstrapSchemaVersion2 ||
        bootstrap.loaded->record.state !=
            ConfigurationBootstrapState::Initialized ||
        !bootstrap.loaded->record.previousEpoch.has_value() ||
        !bootstrap.loaded->record.currentEpoch.has_value() ||
        bootstrap.loaded->record.previousEpoch.value() !=
            proof.previousEpoch() ||
        bootstrap.loaded->record.currentEpoch.value() != proof.currentEpoch()) {
        return mapBootstrapFailure(bootstrap.status);
    }

    const auto current = *bootstrap.loaded;
    if (current.record.handoff != RunEpochHandoffState::Committed) {
        return current.record.handoff == RunEpochHandoffState::Consumed
                   ? makeRejectedWithValidRuntime(
                         ConfigurationRecoveryStatus::StateTransitionRejected)
                   : makeUnavailableResult(ConfigurationRecoveryStatus::
                                               ConfigurationIntegrityFailure);
    }
    const auto consumed = bootstrapStore_.writeHandoffSuccessor(
        current, RunEpochHandoffState::Consumed);
    if (consumed.status != ConfigurationBootstrapWriteStatus::Success ||
        !consumed.loaded.has_value()) {
        return makeUnavailableResult(mapBootstrapWriteFailure(consumed.status));
    }
    proof.markConsumed();
    pendingRunEpochHandoff_.reset();
    return {ConfigurationRecoveryStatus::RuntimeReady, {}};
}

ConfigurationRecoveryResult
ConfigurationRecoveryService::finalizePublishedGraph(
    const LoadedConfigurationBootstrap& bootstrap,
    const PreparedInitialConfigurationGraph& prepared,
    ConfigurationRecoveryStatus successStatus) {
    const auto finalized = bootstrapStore_.writeSuccessor(
        bootstrap, ConfigurationBootstrapState::Initialized);
    return classifyBootstrapFinalization(
        finalized, successStatus,
        [this, &prepared](device_platform::StorageEpoch epoch) {
            return epoch ==
                       prepared.graph.active.manifestReference.storageEpoch &&
                   configurationService_.finalizeRecoveredGraph(
                       epoch, prepared.rootRecordBytes, prepared.planIdentity);
        });
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
        // Same treatment as the equivalent binding check in
        // resolvePendingRoot(): a stale or foreign binding here means the
        // service state moved out from under this attempt, not a simple
        // busy rejection, so it fails closed rather than leaving the
        // service in its current in-progress mode.
        configurationService_.failRecovery(
            ConfigurationRuntimeFailureCause::PublishContractViolation);
        return makeResult(
            ConfigurationRecoveryStatus::ConfigurationIntegrityFailure);
    }
    const auto execution =
        graphStore_.executeInitialGraph(prepared, capability);
    lastResourcePeaks_ = ConfigurationRecoveryResourcePeaks{
        prepared.peakProgramPayloadCapacity,
        prepared.peakDocumentEnvelopeCapacity,
        prepared.peakStoreReadbackCapacity,
        prepared.smallCanonicalRecordCapacity +
            bootstrap.canonicalRecordBytes.capacity(),
        0U,
        configurationService_.fullModelGenerationCount(),
        prepared.peakSlotScanReadCapacity};
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
        lastResourcePeaks_->indeterminateContextCapacity =
            pendingRoot_->bootstrap.canonicalRecordBytes.capacity() +
            pendingRoot_->prepared.rootRecordBytes.capacity();
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
        // A valid old Operational runtime is unaffected by a busy
        // coordinator; without one, callers must be able to tell that no
        // runtime is available right now.
        return makeResetPreparationFailure(
            ConfigurationRecoveryStatus::ConfigurationMutationBusy,
            configurationService_.mode() ==
                ConfigurationServiceMode::Operational);
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
                    armAuthorizedRunEpochHandoff(bootstrap.loaded->record);
                    return {ConfigurationRecoveryStatus::RuntimeReady, {}};
                }
                configurationService_.failRecovery(
                    ConfigurationRuntimeFailureCause::
                        ServiceStateInvariantViolation);
                return {ConfigurationRecoveryStatus::RuntimePreparationFailure,
                        {}};
            }
            const auto successStatus =
                bootstrap.loaded->record.state ==
                        ConfigurationBootstrapState::Initializing
                    ? ConfigurationRecoveryStatus::
                          FactoryInitializationCompleted
                    : ConfigurationRecoveryStatus::FactoryResetCompleted;
            const auto finalized = bootstrapStore_.writeSuccessor(
                *bootstrap.loaded, ConfigurationBootstrapState::Initialized);
            return classifyBootstrapFinalization(
                finalized, successStatus,
                [this](device_platform::StorageEpoch epoch) {
                    return configurationService_
                        .finalizeRecoveredGraphForBootstrap(epoch);
                });
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
        // The two bootstrap reads from the scan above plus the 17 reads
        // just performed by verifyFactoryEmpty() are exactly the 19 known
        // keys; this proof lets the rest of this single attempt skip
        // re-reading any of them. It is bound to this exact store, the
        // lease acquired above, and the revision/generation beginRecovery()
        // just established, and is independently re-validated by every
        // consumer. Neither value changes again before the proof is
        // consumed below.
        const auto boundStateRevision = configurationService_.stateRevision();
        const auto boundRecoveryGeneration =
            configurationService_.recoveryGeneration();
        const FactoryNoveltyProof factoryNoveltyProof(store_, acquired.lease,
                                                      boundStateRevision,
                                                      boundRecoveryGeneration);
        auto prepared = graphStore_.prepareInitialGraph(
            device_platform::StorageEpoch{1U}, decodeChangeOperation(2U),
            &factoryNoveltyProof, &acquired.lease, boundStateRevision,
            boundRecoveryGeneration);
        if (prepared.status != InitialConfigurationPrepareStatus::Success ||
            !prepared.prepared.has_value()) {
            static_cast<void>(configurationService_.cancelRecovery());
            return makeUnavailableResult(mapPrepare(prepared.status));
        }
        auto initial = bootstrapStore_.writeInitialInitializing(
            factoryNoveltyProof, acquired.lease, boundStateRevision,
            boundRecoveryGeneration);
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
        // Only the persisted schema-2 handoff phase can mint the capability.
        // A historical FactoryReset manifest is not sufficient: after the
        // handoff reaches Consumed, later boots must not re-authorize it.
        armAuthorizedRunEpochHandoff(bootstrap.loaded->record);
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
        return makeUnavailableResult(mapRecoveryBeginStatus(begin));
    }
    auto prepared = graphStore_.prepareInitialGraph(
        bootstrap.loaded->record.storageEpoch, operation);
    if (prepared.status != InitialConfigurationPrepareStatus::Success ||
        !prepared.prepared.has_value()) {
        static_cast<void>(configurationService_.cancelRecovery());
        return makeUnavailableResult(mapPrepare(prepared.status));
    }
    return continueEpochBuild(
        *bootstrap.loaded, std::move(*prepared.prepared), operation,
        bootstrap.loaded->record.state ==
                ConfigurationBootstrapState::Initializing
            ? ConfigurationRecoveryStatus::FactoryInitializationCompleted
            : ConfigurationRecoveryStatus::FactoryResetCompleted,
        acquired.lease);
}

// Authorized reset keeps eligibility re-proof, overflow checks and the
// bootstrap-write fail-closed classification in one auditable decision path.
ConfigurationRecoveryResult
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
ConfigurationRecoveryService::beginAuthorizedFactoryReset() {
    auto acquired = mutationCoordinator_.tryAcquire();
    if (acquired.status != ConfigurationMutationAcquireStatus::Acquired) {
        // Same context-aware classification as boot(): a valid old
        // Operational runtime is unaffected by a busy coordinator; without
        // one (NoRuntime or ResetEligibleNoRuntime), no runtime is
        // available right now.
        return makeResetPreparationFailure(
            ConfigurationRecoveryStatus::ConfigurationMutationBusy,
            configurationService_.mode() ==
                ConfigurationServiceMode::Operational);
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
    // A valid Operational runtime can only already be present at entry:
    // nothing below this point ever newly establishes one before the
    // recovery attempt either completes or fails closed.
    const bool oldRuntimeRemainsValid =
        configurationService_.mode() == ConfigurationServiceMode::Operational;
    if (bootstrap.loaded->record.handoff == RunEpochHandoffState::Pending ||
        bootstrap.loaded->record.handoff == RunEpochHandoffState::Committed) {
        // An open run handoff must be resumed and consumed before a later
        // reset can advance the configuration epoch.  Starting a second
        // reset would otherwise discard the only persistent authorization
        // binding for the first handoff.
        return makeResetPreparationFailure(
            ConfigurationRecoveryStatus::StateTransitionRejected,
            oldRuntimeRemainsValid);
    }
    constexpr auto kMaximumResettableEpoch =
        std::numeric_limits<std::uint64_t>::max() / 2U - 1U;
    constexpr auto kRequiredHandoffSuccessors = 4U;
    if (bootstrap.loaded->record.storageEpoch.value() >
            kMaximumResettableEpoch ||
        bootstrap.loaded->record.sequence.value() >
            std::numeric_limits<std::uint64_t>::max() -
                kRequiredHandoffSuccessors ||
        bootstrap.loaded->highWater.value() ==
            std::numeric_limits<std::uint64_t>::max()) {
        return makeResetPreparationFailure(
            ConfigurationRecoveryStatus::CounterOverflow,
            oldRuntimeRemainsValid);
    }
    // Eligibility is re-proven under the lease just acquired above on every
    // call, including when a previous boot() already latched
    // ResetEligibleNoRuntime: that earlier classification is never reused
    // as-is, since the persisted bootstrap or root state may have changed
    // since it was made.
    if (configurationService_.mode() == ConfigurationServiceMode::NoRuntime ||
        configurationService_.mode() ==
            ConfigurationServiceMode::ResetEligibleNoRuntime) {
        const auto graph = graphStore_.loadCanonicalGraph(
            bootstrap.loaded->record.storageEpoch);
        if (!isResetEligibleNoRuntimeGraph(graph)) {
            return makeResult(mapLoad(graph.status), graph.diagnostics);
        }
        if (configurationService_.mode() ==
                ConfigurationServiceMode::NoRuntime &&
            !configurationService_.markResetEligibleNoRuntime()) {
            return makeUnavailableResult(
                ConfigurationRecoveryStatus::StateTransitionRejected);
        }
    }
    if (configurationService_.mode() != ConfigurationServiceMode::Operational &&
        configurationService_.mode() !=
            ConfigurationServiceMode::ResetEligibleNoRuntime) {
        return makeUnavailableResult(
            ConfigurationRecoveryStatus::StateTransitionRejected);
    }
    const auto targetEpoch = device_platform::StorageEpoch{
        bootstrap.loaded->record.storageEpoch.value() + 1U};
    const auto begin = configurationService_.beginRecovery(
        ConfigurationServiceMode::ResetPreparing,
        configuration_limits::kResetRecoveryRevisionHeadroom);
    if (begin != ConfigurationRecoveryBeginStatus::Success) {
        return makeResetPreparationFailure(mapRecoveryBeginStatus(begin),
                                           oldRuntimeRemainsValid);
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
        if (bootstrapFailureLeavesOldState(resetting.status)) {
            static_cast<void>(configurationService_.cancelRecovery());
            return makeResetBootstrapFailure(resetting.status,
                                             oldRuntimeRemainsValid);
        }
        // Integrity failure, unsupported newer schema or a canonical
        // bootstrap that changed underneath this attempt: the old runtime
        // is not provably still backed by the persisted state, so it must
        // not be handed out again. Fail closed instead of cancelling back
        // to Operational.
        configurationService_.failRecovery(
            mapBootstrapWriteFailureCause(resetting.status));
        return makeUnavailableResult(
            mapBootstrapWriteFailure(resetting.status));
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
