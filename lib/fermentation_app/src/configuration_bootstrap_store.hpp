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
    [[nodiscard]] ConfigurationBootstrapWriteResult writeInitialInitializing();
    [[nodiscard]] ConfigurationBootstrapWriteResult writeSuccessor(
        const LoadedConfigurationBootstrap& expected,
        ConfigurationBootstrapState targetState);

   private:
    [[nodiscard]] ConfigurationBootstrapWriteResult writeBound(
        const ConfigurationBootstrapScanResult& scan,
        const ConfigurationBootstrapRecord& target);

    device_platform::IStateStore& store_;
};

}  // namespace fermentation
