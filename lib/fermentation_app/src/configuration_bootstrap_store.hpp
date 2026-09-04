#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "configuration_bootstrap.hpp"
#include "state_store.hpp"
#include "storage_types.hpp"

namespace fermentation {

enum class ConfigurationBootstrapScanStatus : std::uint8_t {
    Empty,
    Available,
    ReadError,
    CapacityError,
    IntegrityFailure,
    UnsupportedNewerSchema,
};

struct LoadedConfigurationBootstrap {
    device_platform::SlotId slot;
    ConfigurationBootstrapRecord record;
    std::string canonicalRecordBytes;
    ConfigurationBootstrapSequence highWater;
    bool duplicate{false};
};

struct ConfigurationBootstrapScanResult {
    ConfigurationBootstrapScanStatus status{
        ConfigurationBootstrapScanStatus::IntegrityFailure};
    std::optional<LoadedConfigurationBootstrap> loaded;
};

enum class ConfigurationBootstrapWriteStatus : std::uint8_t {
    Success,
    InvalidTransition,
    CounterOverflow,
    ReadError,
    CapacityError,
    IntegrityFailure,
    UnsupportedNewerSchema,
    WriteError,
    WriteCapacityError,
    CommitNotEffective,
    BootstrapCommitIndeterminate,
};

struct ConfigurationBootstrapWriteResult {
    ConfigurationBootstrapWriteStatus status{
        ConfigurationBootstrapWriteStatus::IntegrityFailure};
    std::optional<LoadedConfigurationBootstrap> loaded;
};

class ConfigurationBootstrapStore {
   public:
    explicit ConfigurationBootstrapStore(device_platform::IStateStore& store)
        : store_(store) {}

    [[nodiscard]] ConfigurationBootstrapScanResult scan() const;

   private:
    friend class ConfigurationRecoveryService;
    friend class ConfigurationBootstrapStoreTestAccess;
    [[nodiscard]] ConfigurationBootstrapWriteResult writeInitialInitializing();
    // Skips the internal re-scan only if proof.consumeForBootstrapWrite()
    // independently validates that store, mutationLease,
    // serviceStateRevision and recoveryGeneration match exactly what it was
    // constructed for and that graph preparation already consumed it first.
    // On any mismatch, falls back to the always-correct scanning overload
    // above instead of skipping anything.
    [[nodiscard]] ConfigurationBootstrapWriteResult writeInitialInitializing(
        const FactoryNoveltyProof& proof,
        const ConfigurationMutationLease& mutationLease,
        std::uint64_t serviceStateRevision, std::uint64_t recoveryGeneration);
    [[nodiscard]] ConfigurationBootstrapWriteResult writeSuccessor(
        const LoadedConfigurationBootstrap& expected,
        ConfigurationBootstrapState targetState);
    [[nodiscard]] ConfigurationBootstrapWriteResult writeHandoffSuccessor(
        const LoadedConfigurationBootstrap& expected,
        RunEpochHandoffState targetHandoff);
    [[nodiscard]] const device_platform::IStateStore* storeIdentity() const {
        return &store_;
    }
    [[nodiscard]] ConfigurationBootstrapWriteResult writeBound(
        const ConfigurationBootstrapScanResult& scan,
        const ConfigurationBootstrapRecord& target);
    [[nodiscard]] ConfigurationBootstrapWriteResult writeSuccessorWithHandoff(
        const LoadedConfigurationBootstrap& expected,
        ConfigurationBootstrapState targetState,
        RunEpochHandoffState targetHandoff);

    device_platform::IStateStore& store_;
};

}  // namespace fermentation
