#pragma once

#include <cstdint>
#include <memory>
#include <optional>

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
    ConfigurationUnavailable,
    ConfigurationIntegrityFailure,
    UnsupportedNewerConfigurationSchema,
    PersistenceFailure,
    CapacityFailure,
    CounterOverflow,
    RuntimePreparationFailure,
    BootstrapCommitIndeterminate,
    ConfigurationRecordOutcomeIndeterminate,
    ConfigurationCommitIndeterminate,
};

struct ConfigurationRecoveryResult {
    ConfigurationRecoveryStatus status{
        ConfigurationRecoveryStatus::ConfigurationUnavailable};
    ConfigurationGraphDiagnostics diagnostics;
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
};

}  // namespace fermentation
