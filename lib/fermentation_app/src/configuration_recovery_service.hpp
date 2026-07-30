#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_service.hpp"
#include "state_store.hpp"

namespace fermentation {

enum class ConfigurationRecoveryStatus : std::uint8_t {
    RuntimeReady,
    FactoryInitializationCompleted,
    FactoryResetCompleted,
    ConfigurationMutationBusy,
    ConfigurationModelBudgetBusy,
    StateTransitionRejected,
    ConfigurationUnavailable,
    ConfigurationIntegrityFailure,
    UnsupportedNewerConfigurationSchema,
    PersistenceReadFailure,
    PersistenceCapacityFailure,
    PersistenceWriteFailure,
    CounterOverflow,
    RuntimePreparationFailure,
    BootstrapCommitIndeterminate,
    ConfigurationRecordOutcomeIndeterminate,
    ConfigurationCommitIndeterminate,
};

enum class ConfigurationSafetyProducer : std::uint8_t {
    ConfigurationUnavailable,
    ConfigurationIntegrityFailure,
};

struct ConfigurationRecoveryResult {
    ConfigurationRecoveryStatus status{
        ConfigurationRecoveryStatus::ConfigurationUnavailable};
    ConfigurationGraphDiagnostics diagnostics;
    std::optional<ConfigurationSafetyProducer> safetyProducer;
    ConfigurationRecoveryResult() = default;
    ConfigurationRecoveryResult(ConfigurationRecoveryStatus value,
                                ConfigurationGraphDiagnostics graphDiagnostics)
        : status(value), diagnostics(graphDiagnostics) {
        switch (status) {
            case ConfigurationRecoveryStatus::ConfigurationIntegrityFailure:
            case ConfigurationRecoveryStatus::
                UnsupportedNewerConfigurationSchema:
                safetyProducer =
                    ConfigurationSafetyProducer::ConfigurationIntegrityFailure;
                break;
            case ConfigurationRecoveryStatus::ConfigurationUnavailable:
            case ConfigurationRecoveryStatus::PersistenceReadFailure:
            case ConfigurationRecoveryStatus::PersistenceCapacityFailure:
            case ConfigurationRecoveryStatus::PersistenceWriteFailure:
            case ConfigurationRecoveryStatus::BootstrapCommitIndeterminate:
            case ConfigurationRecoveryStatus::
                ConfigurationRecordOutcomeIndeterminate:
            case ConfigurationRecoveryStatus::ConfigurationCommitIndeterminate:
            case ConfigurationRecoveryStatus::RuntimePreparationFailure:
                safetyProducer =
                    ConfigurationSafetyProducer::ConfigurationUnavailable;
                break;
            default:
                break;
        }
    }
};

struct ConfigurationRecoveryResourcePeaks {
    std::size_t programPayloadCapacity{0U};
    std::size_t documentEnvelopeCapacity{0U};
    std::size_t storeReadbackCapacity{0U};
    std::size_t smallCanonicalRecordCapacity{0U};
    std::size_t indeterminateContextCapacity{0U};
    std::size_t fullModelGenerations{0U};
    // Largest transient buffer read while scanning candidate slots for a
    // safe or exact target (e.g. a leftover full-size old ProgramCatalog
    // envelope during a reset), distinct from storeReadbackCapacity.
    std::size_t slotScanReadCapacity{0U};
};

class ConfigurationRecoveryService {
   public:
    [[nodiscard]] static std::unique_ptr<ConfigurationRecoveryService> create(
        device_platform::IStateStore& store,
        ConfigurationBootstrapStore& bootstrapStore,
        ConfigurationGraphStore& graphStore,
        ConfigurationService& configurationService,
        ConfigurationMutationCoordinator& mutationCoordinator);
    ConfigurationRecoveryService(const ConfigurationRecoveryService&) = delete;
    ConfigurationRecoveryService& operator=(
        const ConfigurationRecoveryService&) = delete;
    ConfigurationRecoveryService(ConfigurationRecoveryService&&) = delete;
    ConfigurationRecoveryService& operator=(ConfigurationRecoveryService&&) =
        delete;
    ~ConfigurationRecoveryService() = default;

    [[nodiscard]] ConfigurationRecoveryResult boot();
    [[nodiscard]] ConfigurationRecoveryResult beginAuthorizedFactoryReset();
    [[nodiscard]] std::optional<ConfigurationRecoveryResourcePeaks>
    lastResourcePeaks() const {
        return lastResourcePeaks_;
    }

   private:
    ConfigurationRecoveryService(
        device_platform::IStateStore& store,
        ConfigurationBootstrapStore& bootstrapStore,
        ConfigurationGraphStore& graphStore,
        ConfigurationService& configurationService,
        ConfigurationMutationCoordinator& mutationCoordinator)
        : store_(store),
          bootstrapStore_(bootstrapStore),
          graphStore_(graphStore),
          configurationService_(configurationService),
          mutationCoordinator_(mutationCoordinator) {}

    struct PendingRootResolution {
        PreparedInitialConfigurationGraph prepared;
        LoadedConfigurationBootstrap bootstrap;
        ChangeOperation operation;
        ConfigurationRecoveryStatus successStatus;
        std::uint64_t recoveryGeneration{0U};
        std::uint64_t serviceStateRevision{0U};
    };
    [[nodiscard]] ConfigurationRecoveryResult continueEpochBuild(
        const LoadedConfigurationBootstrap& bootstrap,
        PreparedInitialConfigurationGraph&& prepared, ChangeOperation operation,
        ConfigurationRecoveryStatus successStatus,
        ConfigurationMutationLease& lease);
    [[nodiscard]] ConfigurationRecoveryResult resolvePendingRoot(
        ConfigurationMutationLease& lease);
    [[nodiscard]] ConfigurationRecoveryResult finalizePublishedGraph(
        const LoadedConfigurationBootstrap& bootstrap,
        const PreparedInitialConfigurationGraph& prepared,
        ConfigurationRecoveryStatus successStatus);
    [[nodiscard]] ConfigurationRecoveryStatus verifyFactoryEmpty() const;
    [[nodiscard]] static ConfigurationRecoveryResult mapBootstrapFailure(
        ConfigurationBootstrapScanStatus status);

    device_platform::IStateStore& store_;
    ConfigurationBootstrapStore& bootstrapStore_;
    ConfigurationGraphStore& graphStore_;
    ConfigurationService& configurationService_;
    ConfigurationMutationCoordinator& mutationCoordinator_;
    std::optional<PendingRootResolution> pendingRoot_;
    std::optional<ConfigurationRecoveryResourcePeaks> lastResourcePeaks_;
};

}  // namespace fermentation
